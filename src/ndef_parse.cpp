/*
 * ndef_parse の実装 (仕様書 F-03)
 */
#include "ndef_parse.h"

namespace nfccmd {

namespace {

// TLV タグ
constexpr uint8_t kTlvNull        = 0x00;  //!< 読み飛ばす 1 バイトの詰め物
constexpr uint8_t kTlvNdefMessage = 0x03;  //!< NDEF Message
constexpr uint8_t kTlvTerminator  = 0xFE;  //!< データ領域の終端

// NDEF Record のフラグ
constexpr uint8_t kFlagMessageEnd = 0x40;  //!< ME: 最後のレコード
constexpr uint8_t kFlagShort      = 0x10;  //!< SR: ペイロード長が 1 バイト
constexpr uint8_t kFlagIdLength   = 0x08;  //!< IL: ID 長フィールドを持つ
constexpr uint8_t kTnfMask        = 0x07;
constexpr uint8_t kTnfWellKnown   = 0x01;

}  // namespace

ByteSpan findNdefMessageTlv(const uint8_t* data, const uint32_t len)
{
    if (data == nullptr || len == 0) {
        return ByteSpan{};
    }

    uint32_t pos = 0;
    while (pos < len) {
        const uint8_t tag = data[pos];

        if (tag == kTlvTerminator) {
            break;
        }
        if (tag == kTlvNull) {
            // 長さフィールドを持たない
            ++pos;
            continue;
        }

        if (pos + 1 >= len) {
            break;  // 長さフィールドが無い
        }

        uint32_t value_len  = data[pos + 1];
        uint32_t header_len = 2;
        if (value_len == 0xFF) {
            // 続く 2 バイトがビッグエンディアンの長さ
            if (pos + 3 >= len) {
                break;
            }
            value_len  = (static_cast<uint32_t>(data[pos + 2]) << 8) | data[pos + 3];
            header_len = 4;
        }

        const uint32_t value_pos = pos + header_len;
        if (value_pos + value_len > len) {
            break;  // 宣言された長さがバッファを超えている
        }

        if (tag == kTlvNdefMessage) {
            ByteSpan span;
            span.data = data + value_pos;
            span.size = value_len;
            return span;
        }

        pos = value_pos + value_len;
    }

    return ByteSpan{};
}

ByteSpan findTextRecordPayload(const uint8_t* message, const uint32_t len)
{
    if (message == nullptr || len == 0) {
        return ByteSpan{};
    }

    uint32_t pos = 0;
    while (pos < len) {
        const uint8_t flags = message[pos];
        const bool is_short  = (flags & kFlagShort) != 0;
        const bool has_id    = (flags & kFlagIdLength) != 0;
        const uint8_t tnf    = flags & kTnfMask;

        uint32_t p = pos + 1;
        if (p >= len) {
            break;
        }
        const uint32_t type_len = message[p++];

        uint32_t payload_len = 0;
        if (is_short) {
            if (p >= len) {
                break;
            }
            payload_len = message[p++];
        } else {
            if (p + 4 > len) {
                break;
            }
            payload_len = (static_cast<uint32_t>(message[p]) << 24) |
                          (static_cast<uint32_t>(message[p + 1]) << 16) |
                          (static_cast<uint32_t>(message[p + 2]) << 8) | message[p + 3];
            p += 4;
        }

        uint32_t id_len = 0;
        if (has_id) {
            if (p >= len) {
                break;
            }
            id_len = message[p++];
        }

        const uint32_t type_pos = p;
        if (type_pos + type_len > len) {
            break;
        }
        const uint32_t id_pos = type_pos + type_len;
        if (id_pos + id_len > len) {
            break;
        }
        const uint32_t payload_pos = id_pos + id_len;
        if (payload_pos + payload_len > len) {
            break;  // 宣言されたペイロード長がバッファを超えている
        }

        // RTD Text レコード: TNF が Well Known で type が "T"
        if (tnf == kTnfWellKnown && type_len == 1 && message[type_pos] == 'T') {
            ByteSpan span;
            span.data = message + payload_pos;
            span.size = payload_len;
            return span;
        }

        if ((flags & kFlagMessageEnd) != 0) {
            break;  // 最後のレコードだった
        }
        pos = payload_pos + payload_len;
    }

    return ByteSpan{};
}

}  // namespace nfccmd
