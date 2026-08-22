/*
 * nfc_reader の実装
 *
 * M5Unit-NFC / M5UnitUnified への依存はこのファイルに閉じ込める。
 */
#include "nfc_reader.h"

#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>
#include <wiring/m5_unit_unified_wiring.hpp>

#include <Wire.h>

#include <cstring>
#include <vector>

#include "ndef_parse.h"
#include "ndef_text.h"

namespace nfccmd {

namespace {

// 制御対象のハードウェアは 1 台のため、ここで単一のインスタンスを保持する
m5::unit::UnitUnified g_units;
m5::unit::UnitNFC g_unit;
m5::nfc::NFCLayerA g_nfc_a{g_unit};

// ユニットを UnitUnified へ登録済みか。
// UnitUnified::add() は登録済みのユニットに対して false を返すため、
// 再初期化のたびに登録し直すことはできない (F-11)
bool g_registered{false};

/*!
  @brief I2C バスを解放する
  @note スレーブがバスを掴んだまま (SDA を LOW に保持) になると、以後の通信が
        すべて失敗する。SCL を 9 回トグルして残りのビットを吐き出させ、
        STOP 条件を送って解放させる。CoreS3 の Port A で報告されている
        SCL 固着への対処でもある (仕様書 §8-4)。
 */
void releaseI2CBus()
{
    const auto pins = m5::unit::wiring::i2cPins(m5::unit::wiring::NessoPort::PortA);
    if (pins.sda < 0 || pins.scl < 0) {
        M5_LOGW("nfc_reader: unknown I2C pins, skip bus release");
        return;
    }
    const int sda = pins.sda;
    const int scl = pins.scl;
    M5_LOGI("nfc_reader: releasing I2C bus (sda=%d scl=%d)", sda, scl);

    Wire.end();

    pinMode(sda, INPUT_PULLUP);
    pinMode(scl, OUTPUT);
    for (int i = 0; i < 9; ++i) {
        digitalWrite(scl, HIGH);
        delayMicroseconds(5);
        digitalWrite(scl, LOW);
        delayMicroseconds(5);
    }
    digitalWrite(scl, HIGH);
    delayMicroseconds(5);

    // STOP 条件: SCL を HIGH に保ったまま SDA を LOW から HIGH へ
    pinMode(sda, OUTPUT);
    digitalWrite(sda, LOW);
    delayMicroseconds(5);
    pinMode(sda, INPUT_PULLUP);
    delayMicroseconds(5);

    // クロックを指定せずに開くと既定値になってしまうため、
    // ユニットに設定されている周波数で開き直す
    const uint32_t clock = g_unit.component_config().clock;
    Wire.begin(sda, scl, clock);
    M5_LOGI("nfc_reader: I2C reopened (clock=%lu)", static_cast<unsigned long>(clock));
}

// 直近に検出したカード。NDEF 読み出し時の再活性化で参照する
m5::nfc::a::PICC g_last_picc{};
bool g_has_last_picc{false};

// MIFARE Classic の NDEF 読み出しに用いる鍵と諸元
using ClassicKey = m5::nfc::a::mifare::classic::Key;
//! MAD (MIFARE Application Directory) の Key A
constexpr ClassicKey kMadKeyA{0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5};
//! NFC Forum が定める NDEF セクタの公開 Key A
constexpr ClassicKey kNdefKeyA{0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7};

constexpr uint8_t kBlocksPerSector = 4;   //!< 1 セクタあたりのブロック数 (1K)
constexpr uint8_t kDataBlocks      = 3;   //!< セクタ内のデータブロック数 (残り 1 つはトレーラ)
constexpr uint8_t kBlockSize       = 16;  //!< 1 ブロックのバイト数
constexpr uint8_t kMad1LastSector  = 15;  //!< MAD1 が扱う最終セクタ

/// MAD のエントリが NDEF セクタを指しているか
/// @note NDEF の AID は 0x03E1。格納順は実装により前後するため両方を受け付ける
bool isNdefAid(const uint8_t* aid)
{
    return (aid[0] == 0x03 && aid[1] == 0xE1) || (aid[0] == 0xE1 && aid[1] == 0x03);
}

/// ライブラリの PICC を CardInfo へ変換する
void toCardInfo(const m5::nfc::a::PICC& picc, CardInfo& out)
{
    out.clear();
    out.uid_size = picc.size;
    std::memcpy(out.uid, picc.uid, sizeof(out.uid));
    out.atqa      = picc.atqa;
    out.sak       = picc.sak;
    out.type_name = picc.typeAsString();
}

}  // namespace

const char* toString(const NdefStatus status)
{
    switch (status) {
        case NdefStatus::Ok:
            return "ok";
        case NdefStatus::NotSupported:
            return "not supported";
        case NdefStatus::ReactivateFailed:
            return "reactivate failed";
        case NdefStatus::FormatCheckFailed:
            return "format check failed";
        case NdefStatus::NotFormatted:
            return "not formatted";
        case NdefStatus::ReadFailed:
            return "read failed";
        case NdefStatus::NoTextRecord:
            return "no text record";
        default:
            return "unknown";
    }
}

bool NfcReader::begin()
{
    ready_ = false;

    // 登録は初回だけ行う。UnitUnified::add() は登録済みのユニットを受け付けず、
    // 再初期化のたびに呼ぶと必ず失敗するためである。
    // clock=0 はユニット自身の既定クロックを使う指定。
    // CoreS3 では PortA (G2=SDA / G1=SCL) の外部 I2C にディスパッチされる。
    if (!g_registered) {
        if (!m5::unit::wiring::addI2C(g_units, g_unit, 0, m5::unit::wiring::NessoPort::PortA)) {
            M5_LOGE("nfc_reader: addI2C failed");
            return false;
        }
        g_registered = true;
    }

    // 未接続なら失敗する。呼び出し側が間隔をあけて再試行する (F-11)
    if (!g_units.begin()) {
        M5_LOGW("nfc_reader: units.begin failed (unit not connected?)");
        return false;
    }

    ready_ = true;
    M5_LOGI("nfc_reader: ready. %s", g_units.debugInfo().c_str());
    return true;
}

void NfcReader::update()
{
    if (!ready_) {
        return;
    }
    g_units.update();
}

PollResult NfcReader::poll(CardInfo& out, const uint32_t timeout_ms)
{
    if (!ready_) {
        return PollResult::NoCard;
    }

    // 複数枚が同時にかざされた場合を検知するため、まとめて検出する
    std::vector<m5::nfc::a::PICC> piccs;
    if (!g_nfc_a.detect(piccs, timeout_ms) || piccs.empty()) {
        return PollResult::NoCard;
    }

    if (piccs.size() > 1) {
        // どのカードを読むべきか決められないため処理しない (仕様書 F-01)
        M5_LOGW("nfc_reader: %u cards detected", static_cast<unsigned>(piccs.size()));
        return PollResult::MultipleCards;
    }

    auto& picc = piccs.front();
    // detect() は SAK による暫定分類しか行わないため、identify() で種別を確定する
    if (!g_nfc_a.identify(picc)) {
        M5_LOGW("nfc_reader: identify failed (atqa=%04X sak=%02X)", picc.atqa, picc.sak);
        return PollResult::IdentifyFailed;
    }

    g_last_picc     = picc;
    g_has_last_picc = true;
    toCardInfo(picc, out);
    return PollResult::Detected;
}

NdefReadResult NfcReader::readNdefFromMifareClassic()
{
    NdefReadResult result;
    result.status = NdefStatus::NotFormatted;

    // MAD はセクタ 0 にあり、専用の Key A で保護されている。
    // 認証に失敗するカードは NDEF フォーマットされていないとみなす。
    if (!g_nfc_a.mifareClassicAuthenticateA(1, kMadKeyA)) {
        M5_LOGI("nfc_reader: MAD authentication failed (not NDEF formatted)");
        return result;
    }

    uint8_t mad1[kBlockSize]{};
    uint8_t mad2[kBlockSize]{};
    if (!g_nfc_a.read16(mad1, 1) || !g_nfc_a.read16(mad2, 2)) {
        M5_LOGW("nfc_reader: failed to read MAD");
        result.status = NdefStatus::ReadFailed;
        return result;
    }

    // MAD のエントリから NDEF セクタを拾う。
    // ブロック 1 のバイト 2 以降がセクタ 1〜7、ブロック 2 がセクタ 8〜15 に対応する
    std::vector<uint8_t> ndef_sectors;
    for (uint8_t sector = 1; sector <= kMad1LastSector; ++sector) {
        const uint8_t* aid = (sector <= 7) ? (mad1 + 2 * sector) : (mad2 + 2 * (sector - 8));
        if (isNdefAid(aid)) {
            ndef_sectors.push_back(sector);
        }
    }
    M5_LOGI("nfc_reader: %u NDEF sector(s) in MAD", static_cast<unsigned>(ndef_sectors.size()));

    if (ndef_sectors.empty()) {
        return result;
    }

    // 各 NDEF セクタのデータブロックを連結する
    std::vector<uint8_t> data;
    data.reserve(ndef_sectors.size() * kDataBlocks * kBlockSize);
    for (const uint8_t sector : ndef_sectors) {
        const uint8_t first_block = static_cast<uint8_t>(sector * kBlocksPerSector);
        if (!g_nfc_a.mifareClassicAuthenticateA(first_block, kNdefKeyA)) {
            M5_LOGW("nfc_reader: NDEF key authentication failed on sector %u", static_cast<unsigned>(sector));
            result.status = NdefStatus::ReadFailed;
            return result;
        }
        for (uint8_t i = 0; i < kDataBlocks; ++i) {
            uint8_t buf[kBlockSize]{};
            if (!g_nfc_a.read16(buf, static_cast<uint8_t>(first_block + i))) {
                M5_LOGW("nfc_reader: failed to read block %u", static_cast<unsigned>(first_block + i));
                result.status = NdefStatus::ReadFailed;
                return result;
            }
            data.insert(data.end(), buf, buf + kBlockSize);
        }
    }

    // 連結したデータを TLV -> Record -> Text の順に解析する
    const auto message = findNdefMessageTlv(data.data(), static_cast<uint32_t>(data.size()));
    if (!message.valid()) {
        M5_LOGI("nfc_reader: no NDEF message TLV");
        result.status = NdefStatus::NoTextRecord;
        return result;
    }

    const auto payload = findTextRecordPayload(message.data, message.size);
    if (!payload.valid()) {
        M5_LOGI("nfc_reader: no text record in NDEF message");
        result.status = NdefStatus::NoTextRecord;
        return result;
    }

    const auto parsed = parseTextRecordPayload(payload.data, payload.size);
    if (!parsed.valid) {
        result.status = NdefStatus::NoTextRecord;
        return result;
    }

    result.text      = parsed.text;
    result.truncated = parsed.truncated;
    result.available = true;
    result.status    = NdefStatus::Ok;
    return result;
}

NdefReadResult NfcReader::readNdefText()
{
    NdefReadResult result;
    result.status = NdefStatus::NotSupported;
    if (!ready_ || !g_has_last_picc) {
        return result;
    }

    // NFCLayerA::identify() は処理の最後に deactivate() を呼ぶため、
    // poll() から戻った時点でカードは非活性状態である。
    // NDEF の通信を行うには、ここで ACTIVE 状態へ戻す必要がある。
    if (!g_nfc_a.reactivate(g_last_picc)) {
        M5_LOGW("nfc_reader: failed to reactivate PICC for NDEF");
        result.status = NdefStatus::ReactivateFailed;
        return result;
    }

    // MIFARE Classic は NFC Forum Tag Type ではないため、NDEFLayer では読めない。
    // MAD とセクタ鍵を用いた専用経路で読み出す (仕様書 F-03)
    if (g_last_picc.isMifareClassic()) {
        return readNdefFromMifareClassic();
    }

    // カード種別から NFC Forum Tag タイプを求める。None は NDEF 非対応を意味する
    const auto ftag = m5::nfc::a::get_nfc_forum_tag_type(g_last_picc.type);
    if (ftag == m5::nfc::NFCForumTag::None) {
        M5_LOGI("nfc_reader: NDEF not supported by this PICC type");
        result.status = NdefStatus::NotSupported;
        return result;
    }

    m5::nfc::ndef::NDEFLayer ndef{g_nfc_a};

    bool formatted{};
    if (!ndef.isValidFormat(formatted, ftag)) {
        M5_LOGW("nfc_reader: isValidFormat failed (ftag:%u)", static_cast<unsigned>(ftag));
        result.status = NdefStatus::FormatCheckFailed;
        return result;
    }
    if (!formatted) {
        // 未フォーマットのカードはエラーではない (F-03)
        M5_LOGI("nfc_reader: not NDEF formatted (ftag:%u)", static_cast<unsigned>(ftag));
        result.status = NdefStatus::NotFormatted;
        return result;
    }

    std::vector<m5::nfc::ndef::TLV> tlvs;
    if (!ndef.read(ftag, tlvs)) {
        M5_LOGW("nfc_reader: failed to read NDEF");
        result.status = NdefStatus::ReadFailed;
        return result;
    }
    M5_LOGI("nfc_reader: NDEF read %u TLV(s)", static_cast<unsigned>(tlvs.size()));

    for (const auto& tlv : tlvs) {
        if (!tlv.isMessageTLV()) {
            continue;
        }
        for (const auto& record : tlv.records()) {
            M5_LOGI("nfc_reader: record tnf:%u type:\"%s\" payload:%u",
                    static_cast<unsigned>(record.tnf()), record.type(),
                    static_cast<unsigned>(record.payloadSize()));

            if (record.tnf() != m5::nfc::ndef::TNF::Wellknown) {
                continue;
            }
            // RTD Text レコードの type は "T"
            if (std::strcmp(record.type(), "T") != 0) {
                continue;
            }

            const auto parsed = parseTextRecordPayload(record.payload(), record.payloadSize());
            if (!parsed.valid) {
                continue;
            }

            // 最初に見つかった Text レコードを採用する (F-03)
            result.text      = parsed.text;
            result.truncated = parsed.truncated;
            result.available = true;
            result.status    = NdefStatus::Ok;
            return result;
        }
    }

    return result;
}

bool NfcReader::isAlive()
{
    if (!ready_) {
        return false;
    }
    // アドレス指定のみの送信で ACK が返るかを見る。カードの有無には依存しない
    Wire.beginTransmission(g_unit.address());
    return Wire.endTransmission() == 0;
}

bool NfcReader::recover()
{
    M5_LOGW("nfc_reader: recovering");
    ready_          = false;
    g_has_last_picc = false;

    releaseI2CBus();
    return begin();
}

void NfcReader::release()
{
    if (!ready_) {
        return;
    }
    g_nfc_a.deactivate();
    g_has_last_picc = false;
}

}  // namespace nfccmd
