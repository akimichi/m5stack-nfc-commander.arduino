/*
 * formatter の実装 (仕様書 F-02 / F-04)
 */
#include "formatter.h"

namespace nfccmd {

namespace {

constexpr char kHexUpper[] = "0123456789ABCDEF";
constexpr char kHexLower[] = "0123456789abcdef";

/// UID のバイト区切りに用いる文字。区切らない場合は 0
char separatorChar(const UidSeparator separator)
{
    switch (separator) {
        case UidSeparator::Colon:
            return ':';
        case UidSeparator::Hyphen:
            return '-';
        case UidSeparator::None:
        default:
            return '\0';
    }
}

}  // namespace

std::string formatUid(const CardInfo& card, const UidCase uid_case, const UidSeparator separator)
{
    if (card.uid_size == 0) {
        return std::string{};
    }

    const char* hex = (uid_case == UidCase::Lower) ? kHexLower : kHexUpper;
    const char sep  = separatorChar(separator);

    std::string s;
    s.reserve(card.uid_size * 3);

    for (uint8_t i = 0; i < card.uid_size; ++i) {
        if (sep != '\0' && i != 0) {
            s += sep;
        }
        // 各バイトは必ず 2 桁で表記する
        s += hex[(card.uid[i] >> 4) & 0x0F];
        s += hex[card.uid[i] & 0x0F];
    }
    return s;
}

std::vector<std::string> buildOutputFields(const CardInfo& card, const std::string& ndef_text,
                                           const std::string& command_text, const FormatConfig& cfg)
{
    std::vector<std::string> fields;

    const std::string uid = formatUid(card, cfg.uid_case, cfg.uid_separator);
    const bool has_uid    = !uid.empty();
    const bool has_ndef   = !ndef_text.empty();

    switch (cfg.mode) {
        case OutputMode::Command:
            // 対応表にあればその文字列、無ければ UID を出す (F-12)
            if (!command_text.empty()) {
                fields.push_back(command_text);
            } else if (has_uid) {
                fields.push_back(uid);
            }
            break;

        case OutputMode::NdefOnly:
            // NDEF を取得できなければ UID にフォールバックする (F-04)
            if (has_ndef) {
                fields.push_back(ndef_text);
            } else if (has_uid) {
                fields.push_back(uid);
            }
            break;

        case OutputMode::UidAndNdef:
            if (has_uid) {
                fields.push_back(uid);
            }
            if (has_ndef) {
                fields.push_back(ndef_text);
            }
            break;

        case OutputMode::UidOnly:
        default:
            if (has_uid) {
                fields.push_back(uid);
            }
            break;
    }

    return fields;
}

}  // namespace nfccmd
