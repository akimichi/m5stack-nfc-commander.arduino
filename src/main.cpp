/*
 * m5stack-nfc-commander
 *
 * M5Stack CoreS3 + NFC Universal Unit (ST25R3916) を用いて、
 * NFC-A カードの情報を USB HID キーボード入力として PC に送出する。
 *
 * 仕様書: docs/specification.md
 *
 * 実装段階: S-9 (エラー処理と自動復旧)
 */
#include <M5Unified.h>

// USB HID キーボード出力には TinyUSB (ARDUINO_USB_MODE=0) が必須である。
// ボード定義 m5stack-cores3.json は ARDUINO_USB_MODE=1 を固定で持つため、
// platformio.ini の build_unflags / build_flags による上書きが効いているかを
// ビルド時に検証する。
#if !defined(ARDUINO_USB_MODE)
#error "ARDUINO_USB_MODE is not defined"
#elif ARDUINO_USB_MODE != 0
#error "ARDUINO_USB_MODE must be 0 (TinyUSB). Check build_unflags in platformio.ini"
#endif

// ARDUINO_USB_CDC_ON_BOOT が未定義だと、Arduino core 側で USB CDC が
// 一切登録されず (USBCDC.cpp が丸ごと無効化され、Serial が UART0 になる)、
// シリアルログが失われる。PlatformIO は build_unflags を build_flags の後に
// 適用するため、両方に同じ文字列を書くと未定義に転落する。
#if !defined(ARDUINO_USB_CDC_ON_BOOT)
#error "ARDUINO_USB_CDC_ON_BOOT is not defined. build_unflags may have removed it"
#endif

#include "debounce.h"
#include "formatter.h"
#include "hid_output.h"
#include "nfc_reader.h"
#include "settings.h"
#include "settings_menu.h"
#include "settings_store.h"
#include "ui.h"

namespace {

constexpr const char* kAppName    = "NFC Commander";
constexpr const char* kAppVersion = "0.1.0";

nfccmd::NfcReader g_reader;
nfccmd::Ui g_ui;
nfccmd::ReadDebouncer g_debouncer;
nfccmd::HidOutput g_hid;
uint32_t g_read_count{0};

// 設定は g_settings に一本化し、各モジュールへは applySettings() で配る (F-10)
nfccmd::Settings g_settings;
nfccmd::SettingsStore g_store;

/// 出力モードの表示ラベル (F-08)
const char* outputModeLabel(const nfccmd::OutputMode mode)
{
    switch (mode) {
        case nfccmd::OutputMode::NdefOnly:
            return "NDEF";
        case nfccmd::OutputMode::UidAndNdef:
            return "UID+NDEF";
        case nfccmd::OutputMode::UidOnly:
        default:
            return "UID";
    }
}

/// 設定を各モジュールへ反映する
void applySettings()
{
    g_hid.config(g_settings.toHidConfig());
    g_debouncer.config(g_settings.toDebouncerConfig());
    g_ui.setModeLabel(outputModeLabel(g_settings.out_mode));
}

// HID 出力の可否 (F-07)。意図しない打鍵を防ぐため、起動直後は無効から始める
bool g_output_enabled{false};

/// 表示中の画面
enum class Screen : uint8_t {
    Main,      //!< 読み取り画面
    Settings,  //!< 設定画面 (F-10)
};
Screen g_screen{Screen::Main};
nfccmd::SettingItem g_selected_item{nfccmd::SettingItem::OutputMode};

// 障害検知と復旧 (F-11)
constexpr uint32_t kReinitIntervalMs   = 2000;  //!< ユニット未接続時に再初期化を試す間隔
constexpr uint32_t kHealthIntervalMs   = 1000;  //!< ユニットの生存確認の間隔
constexpr uint8_t kHealthFailureLimit  = 3;     //!< 復旧動作へ移る連続失敗回数
constexpr uint8_t kBusReleaseEvery     = 3;     //!< 何回に 1 回 I2C バス解放も行うか

uint32_t g_last_reinit_ms{0};
uint32_t g_last_health_ms{0};
uint8_t g_health_failures{0};
uint8_t g_reinit_attempts{0};  //!< 上限で飽和させる。試行が続いても巻き戻らないようにする
uint8_t g_reinit_cycle{0};     //!< バス解放の周期判定用。飽和カウンタとは分けて持つ

// 同じ異常状態が続く間、警告音とメッセージを繰り返さないために保持する。
// 正常検出の連打抑止は ReadDebouncer が担当する (F-06)。
nfccmd::PollResult g_last_result{nfccmd::PollResult::NoCard};

/// ユニットが使える状態になったときの処理 (F-11)
void onUnitReady()
{
    g_health_failures = 0;
    g_reinit_attempts = 0;
    g_reinit_cycle    = 0;
    g_ui.setUnitReady(true);
    g_ui.showMessage("");
    g_ui.beepDetect();
    M5_LOGI("unit is ready");
}

/// ユニットが応答しなくなったときの処理 (F-11)
void onUnitLost()
{
    g_health_failures = 0;
    g_reinit_attempts = 0;
    g_reinit_cycle    = 0;

    M5_LOGW("unit stopped responding");
    g_ui.setUnitReady(false);
    g_ui.showMessage("unit lost. recovering...", true);
    g_ui.beepError();

    // 取りかけのカード情報は破棄する
    g_debouncer.reset();

    if (g_reader.recover()) {
        onUnitReady();
    }
}

/// 設定画面へ入る
void enterSettings()
{
    g_screen        = Screen::Settings;
    g_selected_item = nfccmd::SettingItem::OutputMode;
    g_ui.showSettings(g_settings, g_selected_item);
    g_ui.beepDetect();
    M5_LOGI("settings: opened");
}

/// 設定画面を閉じ、保存して読み取り画面へ戻る
void leaveSettings()
{
    g_settings.clampToValidRange();
    applySettings();
    const bool saved = g_store.save(g_settings);

    g_screen = Screen::Main;
    g_ui.showMainScreen();
    g_ui.setCount(g_read_count);
    g_ui.showMessage(saved ? "settings saved" : "failed to save settings", !saved);
    g_ui.beepDetect();
    M5_LOGI("settings: closed (saved=%d)", static_cast<int>(saved));
}

/// 設定画面の操作を処理する
void handleSettingsScreen(const m5::touch_detail_t& touch)
{
    if (!touch.wasClicked()) {
        return;
    }

    switch (g_ui.settingsButtonAt(touch.x, touch.y)) {
        case 0:  // NEXT: 次の項目へ
            g_selected_item = nfccmd::nextSettingItem(g_selected_item);
            g_ui.showSettings(g_settings, g_selected_item);
            g_ui.beepSuppressed();
            break;

        case 1:  // CHANGE: 値を次の候補へ
            nfccmd::advanceSettingItem(g_settings, g_selected_item);
            g_ui.showSettings(g_settings, g_selected_item);
            g_ui.beepDetect();
            M5_LOGI("settings: %s = %s", nfccmd::settingItemLabel(g_selected_item),
                    nfccmd::settingItemValueText(g_settings, g_selected_item).c_str());
            break;

        case 2:  // BACK: 保存して戻る
            leaveSettings();
            break;

        default:
            break;
    }
}

}  // namespace

void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);

    // USB CDC (Serial) を明示的に初期化する。
    // ARDUINO_USB_CDC_ON_BOOT=1 のとき Arduino core も app_main で呼ぶが、
    // ここで参照しておくことでリンクと静的初期化を確実にする。
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.begin(115200);
#endif

    g_ui.begin();
    g_hid.begin();
    g_ui.setOutputEnabled(g_output_enabled);

    if (g_store.load(g_settings)) {
        M5_LOGI("settings: loaded from NVS");
    } else {
        M5_LOGI("settings: using defaults");
    }
    applySettings();

    M5_LOGI("%s %s started", kAppName, kAppVersion);

    if (g_reader.begin()) {
        g_ui.setUnitReady(true);
        g_ui.beepDetect();
    } else {
        // loop() が一定間隔で再初期化を試み続ける (F-11)
        g_ui.setUnitReady(false);
        g_ui.showMessage("NFC unit not found. check Port A", true);
        g_ui.beepError();
    }
}

void loop()
{
    M5.update();
    g_reader.update();

    const auto touch = M5.Touch.getDetail();

    // 設定画面を表示している間は NFC の読み取りを止める。
    // 設定中に意図しない打鍵が起きるのを防ぐためである
    if (g_screen == Screen::Settings) {
        handleSettingsScreen(touch);
        return;
    }

    if (touch.wasHold()) {
        // 長押しで設定画面を開く (F-10)
        enterSettings();
        return;
    }
    if (touch.wasClicked()) {
        // 短押しで HID 出力の有効 / 無効を切り替える (F-07)
        g_output_enabled = !g_output_enabled;
        g_ui.setOutputEnabled(g_output_enabled);
        g_ui.showMessage(g_output_enabled ? "output enabled" : "output disabled");
        if (g_output_enabled) {
            g_ui.beepDetect();
        } else {
            g_ui.beepSuppressed();
        }
        M5_LOGI("HID output %s", g_output_enabled ? "enabled" : "disabled");
    }

    // ユニットが使えない間は、間隔をあけて再初期化を試みる (F-11)
    if (!g_reader.ready()) {
        if (millis() - g_last_reinit_ms >= kReinitIntervalMs) {
            g_last_reinit_ms = millis();

            // 試行回数は上限で飽和させる。8bit のまま加算し続けると一定時間後に
            // 巻き戻り、下の「電源再投入」の案内が消えてしまう
            if (g_reinit_attempts < UINT8_MAX) {
                ++g_reinit_attempts;
            }
            g_reinit_cycle = static_cast<uint8_t>((g_reinit_cycle + 1) % kBusReleaseEvery);

            // 単に未接続なだけなら begin() で足りる。何度も失敗する場合は
            // バスがスレーブに掴まれている可能性があるため解放も試す
            const bool recovered = (g_reinit_cycle == 0) ? g_reader.recover() : g_reader.begin();
            if (recovered) {
                onUnitReady();
            } else if (g_reinit_attempts >= kBusReleaseEvery * 3) {
                // バス解放を繰り返しても戻らない場合は利用者に判断を促す
                g_ui.showMessage("I2C error. power cycle needed", true);
            }
        }
        delay(50);
        return;
    }

    // ユニットの生存を定期的に確かめる。カードの有無とは無関係に判定する (F-11)
    if (millis() - g_last_health_ms >= kHealthIntervalMs) {
        g_last_health_ms = millis();
        if (g_reader.isAlive()) {
            g_health_failures = 0;
        } else if (++g_health_failures >= kHealthFailureLimit) {
            onUnitLost();
            return;
        }
    }

    nfccmd::CardInfo card;
    const auto result = g_reader.poll(card, g_settings.poll_ms);
    // 異常状態が継続している間、警告を鳴らし続けないための判定
    const bool state_changed = (result != g_last_result);
    g_last_result            = result;

    switch (result) {
        case nfccmd::PollResult::Detected: {
            if (g_debouncer.onCardDetected(card, millis())) {
                // NDEF が要らないモードでは読み出さない。1 枚あたりの処理時間を抑えるためである
                std::string ndef_text{};
                bool ndef_truncated{false};
                auto ndef_status = nfccmd::NdefStatus::Ok;
                if (g_settings.out_mode != nfccmd::OutputMode::UidOnly) {
                    const auto ndef = g_reader.readNdefText();
                    ndef_status     = ndef.status;
                    if (ndef.available) {
                        ndef_text      = ndef.text;
                        ndef_truncated = ndef.truncated;
                        M5_LOGI("NDEF text=\"%s\" truncated=%d", ndef_text.c_str(), static_cast<int>(ndef_truncated));
                    } else {
                        M5_LOGI("NDEF not available: %s", nfccmd::toString(ndef.status));
                    }
                }

                const auto format   = g_settings.toFormatConfig();
                const auto uid_text = nfccmd::formatUid(card, format.uid_case, format.uid_separator);
                const auto fields   = nfccmd::buildOutputFields(card, ndef_text, format);
                ++g_read_count;

                M5_LOGI("CARD uid=%s size=%u type=%s atqa=%04X sak=%02X", uid_text.c_str(), card.uid_size,
                        card.type_name.c_str(), card.atqa, card.sak);

                g_ui.showCard(uid_text, card.type_name, ndef_text);
                g_ui.setCount(g_read_count);

                if (!g_output_enabled) {
                    // 読み取りは行うが PC には送らない (F-07)
                    if (ndef_status != nfccmd::NdefStatus::Ok) {
                        g_ui.showMessage(std::string{"NDEF: "} + nfccmd::toString(ndef_status));
                    } else {
                        g_ui.showMessage("output disabled");
                    }
                    g_ui.beepSuppressed();
                } else {
                    const auto sent = g_hid.send(fields);
                    if (!sent.sent) {
                        // UID は必ず ASCII なので、送れない原因は PC 未接続に限られる
                        M5_LOGW("HID send failed: USB not connected");
                        g_ui.showMessage("PC not connected", true);
                        g_ui.beepWarn();
                    } else if (sent.dropped_bytes != 0) {
                        // 打鍵できない文字を取り除いたことを知らせる (F-05 / F-09)
                        M5_LOGW("HID dropped %u bytes", static_cast<unsigned>(sent.dropped_bytes));
                        g_ui.showMessage(std::to_string(sent.dropped_bytes) + " bytes dropped");
                        g_ui.beepWarn();
                    } else if (ndef_truncated) {
                        // 読み出し上限で切り捨てたことを知らせる (F-03)
                        g_ui.showMessage("NDEF truncated");
                        g_ui.beepWarn();
                    } else if (ndef_status != nfccmd::NdefStatus::Ok) {
                        // NDEF を要求されたが取得できなかった理由を示す。
                        // エラーではないので通常音を鳴らす (F-03)
                        g_ui.showMessage(std::string{"NDEF: "} + nfccmd::toString(ndef_status));
                        g_ui.beepDetect();
                    } else {
                        g_ui.showMessage("");
                        g_ui.beepDetect();
                    }
                }
            }
            g_reader.release();
            break;
        }

        case nfccmd::PollResult::MultipleCards:
            if (state_changed) {
                M5_LOGW("CARD multiple cards detected");
                g_ui.showMessage("multiple cards: place only one", true);
                g_ui.beepWarn();
            }
            g_reader.release();
            break;

        case nfccmd::PollResult::IdentifyFailed:
            if (state_changed) {
                M5_LOGW("CARD identify failed");
                g_ui.showMessage("identify failed", true);
                g_ui.beepWarn();
            }
            g_reader.release();
            break;

        case nfccmd::PollResult::NoCard:
        default:
            // カード離脱の判定は連続回数で行う (F-06-2)
            g_debouncer.onNoCard();
            break;
    }
}
