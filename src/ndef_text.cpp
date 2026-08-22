/*
 * ndef_text の実装 (仕様書 F-03)
 */
#include "ndef_text.h"

namespace nfccmd {

namespace {

constexpr uint8_t kUtf16Bit       = 0x80;  //!< status bit7: 立っていれば UTF-16
constexpr uint8_t kLangLengthMask = 0x3F;  //!< status bit5-0: 言語コードのバイト長

}  // namespace

NdefTextResult parseTextRecordPayload(const uint8_t* payload, const uint32_t len, const uint32_t max_bytes)
{
    NdefTextResult result;

    if (payload == nullptr || len < 1) {
        return result;
    }

    const uint8_t status = payload[0];

    // UTF-16 は HID キーボードで打鍵できないため、取得できなかったものとして扱う
    if ((status & kUtf16Bit) != 0) {
        return result;
    }

    const uint32_t lang_len = status & kLangLengthMask;
    // 言語コードがペイロードに収まらないレコードは壊れている
    if (static_cast<uint32_t>(1) + lang_len > len) {
        return result;
    }

    const uint8_t* body      = payload + 1 + lang_len;
    const uint32_t body_len  = len - 1 - lang_len;
    const uint32_t copy_len  = (body_len > max_bytes) ? max_bytes : body_len;

    result.valid     = true;
    result.truncated = body_len > max_bytes;
    result.text.assign(reinterpret_cast<const char*>(body), copy_len);
    return result;
}

}  // namespace nfccmd
