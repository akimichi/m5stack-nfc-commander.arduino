/*
 * formatter — 読み取り結果から出力フィールド列を組み立てる (仕様書 F-02 / F-04)
 *
 * 区切り文字は文字列に埋め込まず、フィールドの並びとして返す。
 * Tab (0x09) は hid_sanitize が除去する範囲にあるためである。
 * フィールド間の区切りは hid_output がキーとして送出する。
 *
 * ハードウェアに依存しない純粋ロジックとして実装する (§10.1)。
 *
 * 実装段階: S-6
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "nfc_reader.h"

namespace nfccmd {

/// UID の英字表記 (F-02)
enum class UidCase : uint8_t {
    Upper,  //!< 大文字 (既定)
    Lower,  //!< 小文字
};

/// UID のバイト区切り (F-02)
enum class UidSeparator : uint8_t {
    None,    //!< 区切らない (既定)
    Colon,   //!< ':'
    Hyphen,  //!< '-'
};

/// 出力モード (F-04)
enum class OutputMode : uint8_t {
    UidOnly,     //!< UID のみ (既定)
    NdefOnly,    //!< NDEF テキストのみ。取得できなければ UID にフォールバックする
    UidAndNdef,  //!< UID と NDEF テキスト。NDEF が無ければ UID のみ
    Command,     //!< 対応表に登録した文字列。未登録なら UID (F-12)
};

struct FormatConfig {
    OutputMode mode{OutputMode::UidOnly};
    UidCase uid_case{UidCase::Upper};
    UidSeparator uid_separator{UidSeparator::None};
};

/*!
  @brief UID を 16 進文字列に変換する (F-02)
  @note 各バイトは必ず 2 桁で表記する (0x04 は "04")
 */
std::string formatUid(const CardInfo& card, UidCase uid_case, UidSeparator separator);

/*!
  @brief 出力するフィールドの並びを組み立てる (F-04 / F-12)
  @param card 読み取ったカード
  @param ndef_text NDEF から取得したテキスト。取得できなかった場合は空文字列
  @param command_text 対応表に登録された文字列。未登録なら空文字列
  @return 出力フィールドの並び。出力すべきものが無ければ空
 */
std::vector<std::string> buildOutputFields(const CardInfo& card, const std::string& ndef_text,
                                           const std::string& command_text, const FormatConfig& cfg);

}  // namespace nfccmd
