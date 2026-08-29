/*
 * Settings のテスト (仕様書 F-10 / §10.1)
 *
 * NVS が壊れていたり旧版の値が残っていても起動できるよう、
 * 範囲外の項目だけが既定値に戻ることを検証する。
 */
#include <unity.h>

#include "settings.h"

namespace {

using nfccmd::FieldSeparator;
using nfccmd::KeyboardLayout;
using nfccmd::KeySeqMode;
using nfccmd::NonAsciiPolicy;
using nfccmd::OutputMode;
using nfccmd::Settings;
using nfccmd::TerminatorKey;
using nfccmd::UidCase;
using nfccmd::UidSeparator;

}  // namespace

void setUp(void)
{
}

void tearDown(void)
{
}

// --- 既定値 -----------------------------------------------------------------

/// 既定値は仕様書 F-10 のとおりである
void test_default_values(void)
{
    const Settings s;
    TEST_ASSERT_EQUAL(OutputMode::UidOnly, s.out_mode);
    TEST_ASSERT_EQUAL(UidCase::Upper, s.uid_case);
    TEST_ASSERT_EQUAL(UidSeparator::None, s.uid_separator);
    TEST_ASSERT_EQUAL(FieldSeparator::Tab, s.field_separator);
    TEST_ASSERT_EQUAL(TerminatorKey::Enter, s.terminator);
    TEST_ASSERT_EQUAL_UINT16(8, s.key_delay_ms);
    TEST_ASSERT_EQUAL(KeyboardLayout::US, s.layout);
    TEST_ASSERT_EQUAL(NonAsciiPolicy::Drop, s.non_ascii);
    // キーシーケンスは既定 OFF である (F-13 / §8-7)
    TEST_ASSERT_EQUAL(KeySeqMode::Off, s.key_seq);
    TEST_ASSERT_EQUAL_UINT16(500, s.debounce_ms);
    TEST_ASSERT_EQUAL_UINT8(3, s.absent_threshold);
    TEST_ASSERT_EQUAL_UINT16(100, s.poll_ms);
}

/// 既定値は clamp しても変化しない
void test_defaults_survive_clamp(void)
{
    Settings s;
    s.clampToValidRange();

    TEST_ASSERT_EQUAL(OutputMode::UidOnly, s.out_mode);
    TEST_ASSERT_EQUAL_UINT16(8, s.key_delay_ms);
    TEST_ASSERT_EQUAL_UINT16(500, s.debounce_ms);
    TEST_ASSERT_EQUAL_UINT8(3, s.absent_threshold);
    TEST_ASSERT_EQUAL_UINT16(100, s.poll_ms);
}

/// 正当な値は clamp で変更されない
void test_valid_values_are_kept(void)
{
    Settings s;
    s.out_mode         = OutputMode::UidAndNdef;
    s.uid_case         = UidCase::Lower;
    s.uid_separator    = UidSeparator::Hyphen;
    s.layout           = KeyboardLayout::JIS;
    s.non_ascii        = NonAsciiPolicy::Replace;
    s.key_delay_ms     = 20;
    s.debounce_ms      = 1000;
    s.absent_threshold = 5;
    s.poll_ms          = 200;
    s.clampToValidRange();

    TEST_ASSERT_EQUAL(OutputMode::UidAndNdef, s.out_mode);
    TEST_ASSERT_EQUAL(UidCase::Lower, s.uid_case);
    TEST_ASSERT_EQUAL(UidSeparator::Hyphen, s.uid_separator);
    TEST_ASSERT_EQUAL(KeyboardLayout::JIS, s.layout);
    TEST_ASSERT_EQUAL(NonAsciiPolicy::Replace, s.non_ascii);
    TEST_ASSERT_EQUAL_UINT16(20, s.key_delay_ms);
    TEST_ASSERT_EQUAL_UINT16(1000, s.debounce_ms);
    TEST_ASSERT_EQUAL_UINT8(5, s.absent_threshold);
    TEST_ASSERT_EQUAL_UINT16(200, s.poll_ms);
}

// --- 範囲外の値 -------------------------------------------------------------

/// 壊れた列挙値は既定値に戻る
void test_invalid_enums_are_reset(void)
{
    Settings s;
    s.out_mode        = static_cast<OutputMode>(99);
    s.uid_case        = static_cast<UidCase>(99);
    s.uid_separator   = static_cast<UidSeparator>(99);
    s.field_separator = static_cast<FieldSeparator>(99);
    s.terminator      = static_cast<TerminatorKey>(99);
    s.layout          = static_cast<KeyboardLayout>(99);
    s.non_ascii       = static_cast<NonAsciiPolicy>(99);
    s.key_seq         = static_cast<KeySeqMode>(99);
    s.clampToValidRange();

    TEST_ASSERT_EQUAL(OutputMode::UidOnly, s.out_mode);
    TEST_ASSERT_EQUAL(UidCase::Upper, s.uid_case);
    TEST_ASSERT_EQUAL(UidSeparator::None, s.uid_separator);
    TEST_ASSERT_EQUAL(FieldSeparator::Tab, s.field_separator);
    TEST_ASSERT_EQUAL(TerminatorKey::Enter, s.terminator);
    TEST_ASSERT_EQUAL(KeyboardLayout::US, s.layout);
    TEST_ASSERT_EQUAL(NonAsciiPolicy::Drop, s.non_ascii);
    TEST_ASSERT_EQUAL(KeySeqMode::Off, s.key_seq);
}

/// 上限を超えた打鍵間隔は既定値に戻る
void test_key_delay_over_max_is_reset(void)
{
    Settings s;
    s.key_delay_ms = 51;
    s.clampToValidRange();
    TEST_ASSERT_EQUAL_UINT16(8, s.key_delay_ms);
}

/// 境界値: 打鍵間隔の上限と下限は保持される
void test_key_delay_boundaries_are_kept(void)
{
    Settings s;
    s.key_delay_ms = 50;
    s.clampToValidRange();
    TEST_ASSERT_EQUAL_UINT16(50, s.key_delay_ms);

    s.key_delay_ms = 0;  // 0 は「待たない」という正当な設定
    s.clampToValidRange();
    TEST_ASSERT_EQUAL_UINT16(0, s.key_delay_ms);
}

/// 上限を超えたデバウンス時間は既定値に戻る
void test_debounce_over_max_is_reset(void)
{
    Settings s;
    s.debounce_ms = 5001;
    s.clampToValidRange();
    TEST_ASSERT_EQUAL_UINT16(500, s.debounce_ms);
}

/// 境界値: デバウンス時間の上限は保持される
void test_debounce_boundary_is_kept(void)
{
    Settings s;
    s.debounce_ms = 5000;
    s.clampToValidRange();
    TEST_ASSERT_EQUAL_UINT16(5000, s.debounce_ms);
}

/// 離脱判定回数 0 は成立しないため既定値に戻る
void test_absent_threshold_zero_is_reset(void)
{
    Settings s;
    s.absent_threshold = 0;
    s.clampToValidRange();
    TEST_ASSERT_EQUAL_UINT8(3, s.absent_threshold);
}

/// 境界値: 離脱判定回数の上下限は保持される
void test_absent_threshold_boundaries_are_kept(void)
{
    Settings s;
    s.absent_threshold = 1;
    s.clampToValidRange();
    TEST_ASSERT_EQUAL_UINT8(1, s.absent_threshold);

    s.absent_threshold = 20;
    s.clampToValidRange();
    TEST_ASSERT_EQUAL_UINT8(20, s.absent_threshold);

    s.absent_threshold = 21;
    s.clampToValidRange();
    TEST_ASSERT_EQUAL_UINT8(3, s.absent_threshold);
}

/// ポーリング周期が範囲外なら既定値に戻る
void test_poll_ms_out_of_range_is_reset(void)
{
    Settings s;
    s.poll_ms = 49;
    s.clampToValidRange();
    TEST_ASSERT_EQUAL_UINT16(100, s.poll_ms);

    s.poll_ms = 501;
    s.clampToValidRange();
    TEST_ASSERT_EQUAL_UINT16(100, s.poll_ms);
}

/// 範囲外の項目だけが戻り、他の項目は保たれる
void test_only_invalid_field_is_reset(void)
{
    Settings s;
    s.out_mode     = OutputMode::NdefOnly;  // 正当
    s.layout       = KeyboardLayout::JIS;   // 正当
    s.key_delay_ms = 999;                   // 範囲外
    s.clampToValidRange();

    TEST_ASSERT_EQUAL(OutputMode::NdefOnly, s.out_mode);
    TEST_ASSERT_EQUAL(KeyboardLayout::JIS, s.layout);
    TEST_ASSERT_EQUAL_UINT16(8, s.key_delay_ms);
}

// --- 各モジュールの設定への変換 ---------------------------------------------

/// FormatConfig へ正しく渡る
void test_to_format_config(void)
{
    Settings s;
    s.out_mode      = OutputMode::UidAndNdef;
    s.uid_case      = UidCase::Lower;
    s.uid_separator = UidSeparator::Colon;

    const auto cfg = s.toFormatConfig();
    TEST_ASSERT_EQUAL(OutputMode::UidAndNdef, cfg.mode);
    TEST_ASSERT_EQUAL(UidCase::Lower, cfg.uid_case);
    TEST_ASSERT_EQUAL(UidSeparator::Colon, cfg.uid_separator);
}

/// HidOutput::Config へ正しく渡る
void test_to_hid_config(void)
{
    Settings s;
    s.key_delay_ms    = 16;
    s.non_ascii       = NonAsciiPolicy::Replace;
    s.terminator      = TerminatorKey::Tab;
    s.field_separator = FieldSeparator::Comma;
    s.layout          = KeyboardLayout::JIS;

    const auto cfg = s.toHidConfig();
    TEST_ASSERT_EQUAL_UINT16(16, cfg.key_delay_ms);
    TEST_ASSERT_EQUAL(NonAsciiPolicy::Replace, cfg.non_ascii);
    TEST_ASSERT_EQUAL(TerminatorKey::Tab, cfg.terminator);
    TEST_ASSERT_EQUAL(FieldSeparator::Comma, cfg.field_separator);
    TEST_ASSERT_EQUAL(KeyboardLayout::JIS, cfg.layout);
}

/// ReadDebouncer::Config へ正しく渡る
void test_to_debouncer_config(void)
{
    Settings s;
    s.debounce_ms      = 1200;
    s.absent_threshold = 7;

    const auto cfg = s.toDebouncerConfig();
    TEST_ASSERT_EQUAL_UINT32(1200, cfg.debounce_ms);
    TEST_ASSERT_EQUAL_UINT8(7, cfg.absent_threshold);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_default_values);
    RUN_TEST(test_defaults_survive_clamp);
    RUN_TEST(test_valid_values_are_kept);
    RUN_TEST(test_invalid_enums_are_reset);
    RUN_TEST(test_key_delay_over_max_is_reset);
    RUN_TEST(test_key_delay_boundaries_are_kept);
    RUN_TEST(test_debounce_over_max_is_reset);
    RUN_TEST(test_debounce_boundary_is_kept);
    RUN_TEST(test_absent_threshold_zero_is_reset);
    RUN_TEST(test_absent_threshold_boundaries_are_kept);
    RUN_TEST(test_poll_ms_out_of_range_is_reset);
    RUN_TEST(test_only_invalid_field_is_reset);
    RUN_TEST(test_to_format_config);
    RUN_TEST(test_to_hid_config);
    RUN_TEST(test_to_debouncer_config);
    return UNITY_END();
}
