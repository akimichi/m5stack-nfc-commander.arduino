/*
 * settings_menu のテスト (仕様書 F-10 / §10.1)
 */
#include <unity.h>

#include <string>

#include "settings_menu.h"

namespace {

using nfccmd::advanceSettingItem;
using nfccmd::kSettingItemCount;
using nfccmd::KeyboardLayout;
using nfccmd::NonAsciiPolicy;
using nfccmd::nextSettingItem;
using nfccmd::OutputMode;
using nfccmd::SettingItem;
using nfccmd::settingItemLabel;
using nfccmd::settingItemValueText;
using nfccmd::Settings;
using nfccmd::UidCase;

}  // namespace

void setUp(void)
{
}

void tearDown(void)
{
}

// --- 項目とラベル -----------------------------------------------------------

/// 全項目にラベルがある
void test_all_items_have_labels(void)
{
    for (uint8_t i = 0; i < kSettingItemCount; ++i) {
        const char* label = settingItemLabel(static_cast<SettingItem>(i));
        TEST_ASSERT_NOT_NULL(label);
        TEST_ASSERT_TRUE_MESSAGE(label[0] != '\0', "every item must have a non-empty label");
    }
}

/// 全項目に値の表示文字列がある
void test_all_items_have_value_text(void)
{
    const Settings s;
    for (uint8_t i = 0; i < kSettingItemCount; ++i) {
        const auto text = settingItemValueText(s, static_cast<SettingItem>(i));
        TEST_ASSERT_FALSE_MESSAGE(text.empty(), "every item must render its value");
    }
}

/// 項目は順に進み、末尾から先頭へ戻る
void test_item_navigation_wraps(void)
{
    TEST_ASSERT_EQUAL(SettingItem::UidCase, nextSettingItem(SettingItem::OutputMode));

    const auto last = static_cast<SettingItem>(kSettingItemCount - 1);
    TEST_ASSERT_EQUAL(SettingItem::OutputMode, nextSettingItem(last));
}

/// 全項目を巡回すると元の項目に戻る
void test_item_navigation_full_cycle(void)
{
    auto item = SettingItem::OutputMode;
    for (uint8_t i = 0; i < kSettingItemCount; ++i) {
        item = nextSettingItem(item);
    }
    TEST_ASSERT_EQUAL(SettingItem::OutputMode, item);
}

// --- 値の巡回 ---------------------------------------------------------------

/// 出力モードは 3 つの値を巡回する
void test_output_mode_cycles(void)
{
    Settings s;
    TEST_ASSERT_EQUAL(OutputMode::UidOnly, s.out_mode);

    advanceSettingItem(s, SettingItem::OutputMode);
    TEST_ASSERT_EQUAL(OutputMode::NdefOnly, s.out_mode);

    advanceSettingItem(s, SettingItem::OutputMode);
    TEST_ASSERT_EQUAL(OutputMode::UidAndNdef, s.out_mode);

    advanceSettingItem(s, SettingItem::OutputMode);
    TEST_ASSERT_EQUAL(OutputMode::UidOnly, s.out_mode);  // 先頭へ戻る
}

/// 2 値の項目は交互に切り替わる
void test_two_value_items_toggle(void)
{
    Settings s;
    advanceSettingItem(s, SettingItem::Layout);
    TEST_ASSERT_EQUAL(KeyboardLayout::JIS, s.layout);
    advanceSettingItem(s, SettingItem::Layout);
    TEST_ASSERT_EQUAL(KeyboardLayout::US, s.layout);

    advanceSettingItem(s, SettingItem::UidCase);
    TEST_ASSERT_EQUAL(UidCase::Lower, s.uid_case);
    advanceSettingItem(s, SettingItem::UidCase);
    TEST_ASSERT_EQUAL(UidCase::Upper, s.uid_case);

    advanceSettingItem(s, SettingItem::NonAscii);
    TEST_ASSERT_EQUAL(NonAsciiPolicy::Replace, s.non_ascii);
    advanceSettingItem(s, SettingItem::NonAscii);
    TEST_ASSERT_EQUAL(NonAsciiPolicy::Drop, s.non_ascii);
}

/*!
  数値項目は候補を巡回し、いずれ元の値へ戻る。
  候補の個数は項目ごとに異なるため、決め打ちの回数ではなく
  「有限回で戻ること」を確かめる。
 */
void test_numeric_items_cycle_back(void)
{
    constexpr int kMaxSteps = 32;

    {
        Settings s;
        const uint16_t start = s.key_delay_ms;
        bool returned        = false;
        for (int i = 0; i < kMaxSteps && !returned; ++i) {
            advanceSettingItem(s, SettingItem::KeyDelay);
            returned = (s.key_delay_ms == start);
        }
        TEST_ASSERT_TRUE_MESSAGE(returned, "key delay must cycle back");
    }
    {
        Settings s;
        const uint16_t start = s.debounce_ms;
        bool returned        = false;
        for (int i = 0; i < kMaxSteps && !returned; ++i) {
            advanceSettingItem(s, SettingItem::Debounce);
            returned = (s.debounce_ms == start);
        }
        TEST_ASSERT_TRUE_MESSAGE(returned, "debounce must cycle back");
    }
    {
        Settings s;
        const uint8_t start = s.absent_threshold;
        bool returned       = false;
        for (int i = 0; i < kMaxSteps && !returned; ++i) {
            advanceSettingItem(s, SettingItem::AbsentThreshold);
            returned = (s.absent_threshold == start);
        }
        TEST_ASSERT_TRUE_MESSAGE(returned, "absent threshold must cycle back");
    }
    {
        Settings s;
        const uint16_t start = s.poll_ms;
        bool returned        = false;
        for (int i = 0; i < kMaxSteps && !returned; ++i) {
            advanceSettingItem(s, SettingItem::PollInterval);
            returned = (s.poll_ms == start);
        }
        TEST_ASSERT_TRUE_MESSAGE(returned, "poll interval must cycle back");
    }
}

/// 数値項目を進めると実際に値が変わる
void test_numeric_items_actually_change(void)
{
    Settings s;
    const uint16_t before = s.key_delay_ms;
    advanceSettingItem(s, SettingItem::KeyDelay);
    TEST_ASSERT_NOT_EQUAL(before, s.key_delay_ms);
}

/*!
  値の候補はすべて F-10 の許容範囲に収まっていなければならない。
  設定画面で選んだ値が clampToValidRange() で既定値に戻されると、
  操作しても値が変わらないように見えてしまう。
 */
void test_all_choices_stay_within_valid_range(void)
{
    for (uint8_t i = 0; i < kSettingItemCount; ++i) {
        const auto item = static_cast<SettingItem>(i);
        Settings s;
        // 各項目を十分な回数進め、そのたびに範囲内であることを確かめる
        for (int n = 0; n < 24; ++n) {
            advanceSettingItem(s, item);

            Settings clamped = s;
            clamped.clampToValidRange();

            TEST_ASSERT_EQUAL_UINT16_MESSAGE(s.key_delay_ms, clamped.key_delay_ms, "key_delay out of range");
            TEST_ASSERT_EQUAL_UINT16_MESSAGE(s.debounce_ms, clamped.debounce_ms, "debounce out of range");
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(s.absent_threshold, clamped.absent_threshold, "absent out of range");
            TEST_ASSERT_EQUAL_UINT16_MESSAGE(s.poll_ms, clamped.poll_ms, "poll_ms out of range");
            TEST_ASSERT_EQUAL_MESSAGE(s.out_mode, clamped.out_mode, "out_mode out of range");
            TEST_ASSERT_EQUAL_MESSAGE(s.terminator, clamped.terminator, "terminator out of range");
            TEST_ASSERT_EQUAL_MESSAGE(s.field_separator, clamped.field_separator, "field_sep out of range");
            TEST_ASSERT_EQUAL_MESSAGE(s.uid_separator, clamped.uid_separator, "uid_sep out of range");
        }
    }
}

/// ある項目を変更しても、他の項目は影響を受けない
void test_advancing_one_item_does_not_affect_others(void)
{
    Settings s;
    advanceSettingItem(s, SettingItem::KeyDelay);

    TEST_ASSERT_EQUAL(OutputMode::UidOnly, s.out_mode);
    TEST_ASSERT_EQUAL(KeyboardLayout::US, s.layout);
    TEST_ASSERT_EQUAL_UINT16(500, s.debounce_ms);
    TEST_ASSERT_EQUAL_UINT16(100, s.poll_ms);
}

// --- 表示文字列 -------------------------------------------------------------

/// 値の表示は変更に追従する
void test_value_text_follows_change(void)
{
    Settings s;
    const auto before = settingItemValueText(s, SettingItem::OutputMode);
    advanceSettingItem(s, SettingItem::OutputMode);
    const auto after = settingItemValueText(s, SettingItem::OutputMode);

    TEST_ASSERT_TRUE(before != after);
}

/// 数値項目には単位が付く
void test_numeric_value_text_has_unit(void)
{
    const Settings s;
    const auto text = settingItemValueText(s, SettingItem::KeyDelay);
    TEST_ASSERT_TRUE_MESSAGE(text.find("ms") != std::string::npos, "key delay should show unit");
    TEST_ASSERT_TRUE_MESSAGE(text.find("8") != std::string::npos, "key delay should show value");
}

/// 異常系: 範囲外の項目 ID を渡しても壊れない
void test_invalid_item_is_safe(void)
{
    Settings s;
    const auto invalid = static_cast<SettingItem>(99);

    TEST_ASSERT_NOT_NULL(settingItemLabel(invalid));
    settingItemValueText(s, invalid);
    advanceSettingItem(s, invalid);

    // 設定は既定値のまま変化しない
    TEST_ASSERT_EQUAL(OutputMode::UidOnly, s.out_mode);
    TEST_ASSERT_EQUAL_UINT16(8, s.key_delay_ms);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_all_items_have_labels);
    RUN_TEST(test_all_items_have_value_text);
    RUN_TEST(test_item_navigation_wraps);
    RUN_TEST(test_item_navigation_full_cycle);
    RUN_TEST(test_output_mode_cycles);
    RUN_TEST(test_two_value_items_toggle);
    RUN_TEST(test_numeric_items_cycle_back);
    RUN_TEST(test_numeric_items_actually_change);
    RUN_TEST(test_all_choices_stay_within_valid_range);
    RUN_TEST(test_advancing_one_item_does_not_affect_others);
    RUN_TEST(test_value_text_follows_change);
    RUN_TEST(test_numeric_value_text_has_unit);
    RUN_TEST(test_invalid_item_is_safe);
    return UNITY_END();
}
