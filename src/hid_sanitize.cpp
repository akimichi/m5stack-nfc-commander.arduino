/*
 * hid_sanitize の実装 (仕様書 F-05)
 */
#include "hid_sanitize.h"

namespace nfccmd {

namespace {

/// HID キーボードで打鍵できる文字か (ASCII 印字可能文字)
inline bool isPrintableAscii(const unsigned char c)
{
    return c >= 0x20 && c <= 0x7E;
}

}  // namespace

SanitizeResult sanitizeForHid(const std::string& in, const NonAsciiPolicy policy)
{
    SanitizeResult result;
    result.text.reserve(in.size());

    // 打鍵不能バイトが連続しているか。UTF-8 の 1 文字が複数の '?' に
    // ならないよう、連続した区間を 1 個にまとめるために用いる。
    bool in_invalid_run = false;

    for (const unsigned char c : in) {
        if (isPrintableAscii(c)) {
            result.text += static_cast<char>(c);
            in_invalid_run = false;
            continue;
        }

        ++result.dropped_bytes;
        if (policy == NonAsciiPolicy::Replace && !in_invalid_run) {
            result.text += '?';
        }
        in_invalid_run = true;
    }

    return result;
}

}  // namespace nfccmd
