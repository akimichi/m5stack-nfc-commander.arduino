/*
 * settings の実装 (仕様書 F-10)
 */
#include "settings.h"

namespace nfccmd {

namespace {

/*!
  @brief 列挙値が範囲内かを確かめ、外れていれば既定値を返す
  @param value 検査する値
  @param max_value 取りうる最大の列挙値
  @param default_value 範囲外だったときに用いる値
 */
template <typename E>
E clampEnum(const E value, const E max_value, const E default_value)
{
    const auto v = static_cast<uint8_t>(value);
    return (v <= static_cast<uint8_t>(max_value)) ? value : default_value;
}

/// 数値が範囲内かを確かめ、外れていれば既定値を返す
template <typename T>
T clampNumber(const T value, const T min_value, const T max_value, const T default_value)
{
    return (value >= min_value && value <= max_value) ? value : default_value;
}

}  // namespace

void Settings::clampToValidRange()
{
    // 列挙値。NVS から読んだ生の値が定義域を外れている場合に備える
    out_mode        = clampEnum(out_mode, OutputMode::Command, OutputMode::UidOnly);
    uid_case        = clampEnum(uid_case, UidCase::Lower, UidCase::Upper);
    uid_separator   = clampEnum(uid_separator, UidSeparator::Hyphen, UidSeparator::None);
    field_separator = clampEnum(field_separator, FieldSeparator::Comma, FieldSeparator::Tab);
    terminator      = clampEnum(terminator, TerminatorKey::Tab, TerminatorKey::Enter);
    layout          = clampEnum(layout, KeyboardLayout::JIS, KeyboardLayout::US);
    non_ascii       = clampEnum(non_ascii, NonAsciiPolicy::Replace, NonAsciiPolicy::Drop);
    key_seq         = clampEnum(key_seq, KeySeqMode::On, KeySeqMode::Off);

    // 数値。0 が正当な設定である項目 (key_delay_ms, debounce_ms) は下限を 0 とする
    key_delay_ms = clampNumber<uint16_t>(key_delay_ms, 0, SettingsLimits::kKeyDelayMax, 8);
    debounce_ms  = clampNumber<uint16_t>(debounce_ms, 0, SettingsLimits::kDebounceMax, 500);
    absent_threshold =
        clampNumber<uint8_t>(absent_threshold, SettingsLimits::kAbsentMin, SettingsLimits::kAbsentMax, 3);
    poll_ms = clampNumber<uint16_t>(poll_ms, SettingsLimits::kPollMsMin, SettingsLimits::kPollMsMax, 100);
}

FormatConfig Settings::toFormatConfig() const
{
    FormatConfig cfg;
    cfg.mode          = out_mode;
    cfg.uid_case      = uid_case;
    cfg.uid_separator = uid_separator;
    return cfg;
}

HidOutput::Config Settings::toHidConfig() const
{
    HidOutput::Config cfg;
    cfg.key_delay_ms    = key_delay_ms;
    cfg.non_ascii       = non_ascii;
    cfg.terminator      = terminator;
    cfg.field_separator = field_separator;
    cfg.layout          = layout;
    return cfg;
}

ReadDebouncer::Config Settings::toDebouncerConfig() const
{
    ReadDebouncer::Config cfg;
    cfg.debounce_ms      = debounce_ms;
    cfg.absent_threshold = absent_threshold;
    return cfg;
}

}  // namespace nfccmd
