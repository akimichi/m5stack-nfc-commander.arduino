/*
 * settings_menu の実装 (仕様書 F-10)
 */
#include "settings_menu.h"

namespace nfccmd {

namespace {

// 数値項目の候補。いずれも F-10 の許容範囲に収める
constexpr uint16_t kKeyDelayChoices[] = {0, 4, 8, 16, 32, 50};
constexpr uint16_t kDebounceChoices[] = {0, 250, 500, 1000, 2000, 5000};
constexpr uint8_t kAbsentChoices[]    = {1, 2, 3, 5, 10, 20};
constexpr uint16_t kPollChoices[]     = {50, 100, 200, 300, 500};

/*!
  @brief 候補の並びから次の値を返す
  @note 現在値が候補に無い場合は先頭の候補へ移す
 */
template <typename T, size_t N>
T nextChoice(const T current, const T (&choices)[N])
{
    for (size_t i = 0; i < N; ++i) {
        if (choices[i] == current) {
            return choices[(i + 1) % N];
        }
    }
    return choices[0];
}

/// 列挙値を次へ進める (最大値の次は先頭へ戻る)
template <typename E>
E nextEnum(const E value, const E max_value)
{
    const auto v   = static_cast<uint8_t>(value);
    const auto max = static_cast<uint8_t>(max_value);
    return static_cast<E>((v >= max) ? 0 : (v + 1));
}

}  // namespace

const char* settingItemLabel(const SettingItem item)
{
    switch (item) {
        case SettingItem::OutputMode:
            return "Output mode";
        case SettingItem::UidCase:
            return "UID case";
        case SettingItem::UidSeparator:
            return "UID separator";
        case SettingItem::FieldSeparator:
            return "Field separator";
        case SettingItem::Terminator:
            return "Terminator";
        case SettingItem::KeyDelay:
            return "Key delay";
        case SettingItem::Layout:
            return "Layout";
        case SettingItem::NonAscii:
            return "Non-ASCII";
        case SettingItem::Debounce:
            return "Debounce";
        case SettingItem::AbsentThreshold:
            return "Absent count";
        case SettingItem::PollInterval:
            return "Poll interval";
        default:
            return "-";
    }
}

std::string settingItemValueText(const Settings& settings, const SettingItem item)
{
    switch (item) {
        case SettingItem::OutputMode:
            switch (settings.out_mode) {
                case OutputMode::NdefOnly:
                    return "NDEF";
                case OutputMode::UidAndNdef:
                    return "UID+NDEF";
                case OutputMode::Command:
                    return "COMMAND";
                case OutputMode::UidOnly:
                default:
                    return "UID";
            }

        case SettingItem::UidCase:
            return (settings.uid_case == UidCase::Lower) ? "lower" : "UPPER";

        case SettingItem::UidSeparator:
            switch (settings.uid_separator) {
                case UidSeparator::Colon:
                    return "COLON";
                case UidSeparator::Hyphen:
                    return "HYPHEN";
                case UidSeparator::None:
                default:
                    return "NONE";
            }

        case SettingItem::FieldSeparator:
            switch (settings.field_separator) {
                case FieldSeparator::Space:
                    return "SPACE";
                case FieldSeparator::Comma:
                    return "COMMA";
                case FieldSeparator::Tab:
                default:
                    return "TAB";
            }

        case SettingItem::Terminator:
            switch (settings.terminator) {
                case TerminatorKey::None:
                    return "NONE";
                case TerminatorKey::Tab:
                    return "TAB";
                case TerminatorKey::Enter:
                default:
                    return "ENTER";
            }

        case SettingItem::KeyDelay:
            return std::to_string(settings.key_delay_ms) + " ms";

        case SettingItem::Layout:
            return (settings.layout == KeyboardLayout::JIS) ? "JIS" : "US";

        case SettingItem::NonAscii:
            return (settings.non_ascii == NonAsciiPolicy::Replace) ? "REPLACE" : "DROP";

        case SettingItem::Debounce:
            return std::to_string(settings.debounce_ms) + " ms";

        case SettingItem::AbsentThreshold:
            return std::to_string(settings.absent_threshold);

        case SettingItem::PollInterval:
            return std::to_string(settings.poll_ms) + " ms";

        default:
            return "-";
    }
}

void advanceSettingItem(Settings& settings, const SettingItem item)
{
    switch (item) {
        case SettingItem::OutputMode:
            settings.out_mode = nextEnum(settings.out_mode, OutputMode::Command);
            break;
        case SettingItem::UidCase:
            settings.uid_case = nextEnum(settings.uid_case, UidCase::Lower);
            break;
        case SettingItem::UidSeparator:
            settings.uid_separator = nextEnum(settings.uid_separator, UidSeparator::Hyphen);
            break;
        case SettingItem::FieldSeparator:
            settings.field_separator = nextEnum(settings.field_separator, FieldSeparator::Comma);
            break;
        case SettingItem::Terminator:
            settings.terminator = nextEnum(settings.terminator, TerminatorKey::Tab);
            break;
        case SettingItem::KeyDelay:
            settings.key_delay_ms = nextChoice(settings.key_delay_ms, kKeyDelayChoices);
            break;
        case SettingItem::Layout:
            settings.layout = nextEnum(settings.layout, KeyboardLayout::JIS);
            break;
        case SettingItem::NonAscii:
            settings.non_ascii = nextEnum(settings.non_ascii, NonAsciiPolicy::Replace);
            break;
        case SettingItem::Debounce:
            settings.debounce_ms = nextChoice(settings.debounce_ms, kDebounceChoices);
            break;
        case SettingItem::AbsentThreshold:
            settings.absent_threshold = nextChoice(settings.absent_threshold, kAbsentChoices);
            break;
        case SettingItem::PollInterval:
            settings.poll_ms = nextChoice(settings.poll_ms, kPollChoices);
            break;
        default:
            // 範囲外の項目 ID では何も変更しない
            break;
    }
}

SettingItem nextSettingItem(const SettingItem item)
{
    const auto index = static_cast<uint8_t>(item);
    if (index + 1 >= kSettingItemCount) {
        return SettingItem::OutputMode;
    }
    return static_cast<SettingItem>(index + 1);
}

}  // namespace nfccmd
