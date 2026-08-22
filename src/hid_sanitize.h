/*
 * hid_sanitize — HID キーボードで打鍵可能な文字列への変換 (仕様書 F-05)
 *
 * HID キーボードは ASCII 印字可能文字しか打鍵できないため、
 * 範囲外のバイトを事前に取り除く。ハードウェアに依存しない純粋ロジックとして
 * 実装し、ホスト側テストの対象とする (§10.1)。
 *
 * 実装段階: S-5
 */
#pragma once

#include <cstdint>
#include <string>

namespace nfccmd {

/// 打鍵できないバイトの扱い
enum class NonAsciiPolicy : uint8_t {
    Drop,     //!< 破棄する (既定)
    Replace,  //!< '?' に置換する
};

struct SanitizeResult {
    std::string text{};          //!< 打鍵可能な文字列
    uint16_t dropped_bytes{0};   //!< 取り除いたバイト数
};

/*!
  @brief HID で打鍵できない文字を取り除く
  @param in 入力文字列
  @param policy 打鍵できないバイトの扱い
  @note Replace の場合、連続する打鍵不能バイト列はまとめて 1 個の '?' にする。
        UTF-8 の 1 文字が "???" に化けるのを避けるためである。
 */
SanitizeResult sanitizeForHid(const std::string& in, NonAsciiPolicy policy);

}  // namespace nfccmd
