/*
 * nfc_reader — M5Unit-NFC (ST25R3916) のラッパ
 *
 * 仕様書 §5.3 のモジュール分割における nfc_reader に相当する。
 * M5Unit-NFC は v0.1.0 と新しく API 変更の可能性があるため (§8-5)、
 * ライブラリの型をこのヘッダに露出させず、実装側に隔離する。
 *
 * 実装段階: S-9 (エラー処理と自動復旧)
 */
#pragma once

#include <cstdint>
#include <cstring>
#include <string>

namespace nfccmd {

/// 検出した NFC-A カードの情報
struct CardInfo {
    uint8_t uid[10]{};        //!< UID (先頭 uid_size バイトが有効)
    uint8_t uid_size{};       //!< UID 長 (4 / 7 / 10)
    uint16_t atqa{};          //!< ATQA
    uint8_t sak{};            //!< SAK
    std::string type_name{};  //!< カード種別名 ("NTAG_215" など)

    /// UID 長が NFC-A の規定値かどうか
    bool valid() const
    {
        return uid_size == 4 || uid_size == 7 || uid_size == 10;
    }

    /// UID が同一かどうか (デバウンス判定に用いる)
    bool sameUidAs(const CardInfo& other) const
    {
        return uid_size == other.uid_size && std::memcmp(uid, other.uid, uid_size) == 0;
    }

    void clear()
    {
        *this = CardInfo{};
    }
};

/// NDEF 読み出しの結果種別。取得できない理由を画面とログの両方で切り分けるために持つ
enum class NdefStatus : uint8_t {
    Ok,                 //!< Text レコードを取得できた
    NotSupported,       //!< カード種別が NDEF 非対応
    ReactivateFailed,   //!< カードを ACTIVE 状態に戻せなかった
    FormatCheckFailed,  //!< フォーマット判定の通信に失敗した
    NotFormatted,       //!< NDEF フォーマットされていない
    ReadFailed,         //!< NDEF の読み出しに失敗した
    NoTextRecord,       //!< NDEF はあるが Text レコードが無い
};

/// 結果種別の短い表示名 (画面表示用)
const char* toString(NdefStatus status);

/// NDEF Text の読み出し結果 (F-03)
struct NdefReadResult {
    std::string text{};     //!< 本文 (UTF-8)
    bool available{false};  //!< Text レコードを取得できたか
    bool truncated{false};  //!< 読み出し上限を超えて切り捨てたか
    NdefStatus status{NdefStatus::NotSupported};
};

/// poll() の結果
enum class PollResult : uint8_t {
    NoCard,          //!< カードを検出しなかった
    Detected,        //!< カードを 1 枚検出した
    MultipleCards,   //!< 複数枚を同時に検出した (処理対象外)
    IdentifyFailed,  //!< 検出したが種別の確定に失敗した
};

/*!
  @brief NFC-A カードリーダ
  @note 制御対象のハードウェアは 1 台のため、実装は内部で単一の
        ユニットインスタンスを共有する。複数生成しても意味を持たない。
 */
class NfcReader {
public:
    /// ユニットを初期化する
    bool begin();

    /// 初期化済みかどうか
    bool ready() const
    {
        return ready_;
    }

    /// ユニットの内部状態を更新する (毎ループ呼ぶこと)
    void update();

    /*!
      @brief カードを検出し、成功すれば情報を取得する
      @param out 検出したカード情報の格納先
      @param timeout_ms 検出待ち時間
     */
    PollResult poll(CardInfo& out, uint32_t timeout_ms = 100);

    /*!
      @brief 直近に検出したカードから NDEF Text を読み出す (F-03)
      @note poll() が Detected を返した後、release() を呼ぶ前に使うこと。
            カードが非対応・未フォーマット・Text レコード無しのいずれでも
            エラーとはせず、available=false を返す。
     */
    NdefReadResult readNdefText();

    /*!
      @brief 直近に検出したカードが、まだ電界内に存在するか
      @note カード離脱の検知 (仕様書 F-06) に用いる
     */
    bool isCardStillPresent();

    /// カードを非活性化し、次の検出に備える
    void release();

    ///@name 障害検知と復旧 (F-11)
    ///@{
    /*!
      @brief ユニットが I2C に応答するか調べる
      @note カードの有無とは無関係に、ユニット自体の生存を確認する
     */
    bool isAlive();

    /*!
      @brief I2C バスを解放したうえでユニットを初期化し直す
      @return 復旧できたら true
      @note スレーブがバスを掴んだままの状態 (SCL 固着) を想定し、
            クロックを送って解放させてから再初期化する
     */
    bool recover();
    ///@}

private:
    /// MIFARE Classic 専用の NDEF 読み出し経路 (F-03)
    NdefReadResult readNdefFromMifareClassic();

    bool ready_{false};
};

}  // namespace nfccmd
