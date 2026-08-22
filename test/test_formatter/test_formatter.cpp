/*
 * formatter のテスト (仕様書 F-02 / F-04 / §10.1)
 */
#include <unity.h>

#include "formatter.h"

namespace {

using nfccmd::buildOutputFields;
using nfccmd::CardInfo;
using nfccmd::FormatConfig;
using nfccmd::formatUid;
using nfccmd::OutputMode;
using nfccmd::UidCase;
using nfccmd::UidSeparator;

CardInfo makeCard(std::initializer_list<uint8_t> uid)
{
    CardInfo c;
    c.uid_size = static_cast<uint8_t>(uid.size());
    uint8_t i  = 0;
    for (auto b : uid) {
        c.uid[i++] = b;
    }
    c.type_name = "TEST";
    return c;
}

}  // namespace

void setUp(void)
{
}

void tearDown(void)
{
}

// --- F-02 UID の整形 -------------------------------------------------------

/// 既定は大文字・区切りなし
void test_uid_upper_without_separator(void)
{
    const auto card = makeCard({0xA3, 0x7A, 0x1B, 0xDD});
    TEST_ASSERT_EQUAL_STRING("A37A1BDD", formatUid(card, UidCase::Upper, UidSeparator::None).c_str());
}

/// 小文字表記
void test_uid_lower_case(void)
{
    const auto card = makeCard({0xA3, 0x7A, 0x1B, 0xDD});
    TEST_ASSERT_EQUAL_STRING("a37a1bdd", formatUid(card, UidCase::Lower, UidSeparator::None).c_str());
}

/// コロン区切り。区切りは末尾に付かない
void test_uid_colon_separator(void)
{
    const auto card = makeCard({0xA3, 0x7A, 0x1B, 0xDD});
    TEST_ASSERT_EQUAL_STRING("A3:7A:1B:DD", formatUid(card, UidCase::Upper, UidSeparator::Colon).c_str());
}

/// ハイフン区切り
void test_uid_hyphen_separator(void)
{
    const auto card = makeCard({0xA3, 0x7A, 0x1B, 0xDD});
    TEST_ASSERT_EQUAL_STRING("A3-7A-1B-DD", formatUid(card, UidCase::Upper, UidSeparator::Hyphen).c_str());
}

/// 境界値: 上位ニブルが 0 のバイトも 2 桁で表記する
void test_uid_pads_leading_zero(void)
{
    const auto card = makeCard({0x04, 0x00, 0x0F, 0x10});
    TEST_ASSERT_EQUAL_STRING("04000F10", formatUid(card, UidCase::Upper, UidSeparator::None).c_str());
}

/// 境界値: 7 バイト UID (NTAG / Ultralight 系)
void test_uid_7bytes(void)
{
    const auto card = makeCard({0x04, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66});
    TEST_ASSERT_EQUAL_STRING("04112233445566", formatUid(card, UidCase::Upper, UidSeparator::None).c_str());
}

/// 境界値: 10 バイト UID
void test_uid_10bytes(void)
{
    const auto card = makeCard({0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A});
    TEST_ASSERT_EQUAL_STRING("0102030405060708090A", formatUid(card, UidCase::Upper, UidSeparator::None).c_str());
}

/// 異常系: UID 長が 0 なら空文字列を返す
void test_uid_empty_when_size_zero(void)
{
    CardInfo card;  // uid_size = 0
    TEST_ASSERT_EQUAL_STRING("", formatUid(card, UidCase::Upper, UidSeparator::Colon).c_str());
}

// --- F-04 出力フィールドの組み立て -----------------------------------------

/// UID_ONLY: NDEF があっても UID だけを出力する
void test_fields_uid_only_ignores_ndef(void)
{
    const auto card = makeCard({0xA3, 0x7A, 0x1B, 0xDD});
    FormatConfig cfg;
    cfg.mode = OutputMode::UidOnly;

    const auto fields = buildOutputFields(card, "hello", "", cfg);
    TEST_ASSERT_EQUAL_UINT32(1, fields.size());
    TEST_ASSERT_EQUAL_STRING("A37A1BDD", fields[0].c_str());
}

/// NDEF_ONLY: NDEF テキストがあればそれだけを出力する
void test_fields_ndef_only(void)
{
    const auto card = makeCard({0xA3, 0x7A, 0x1B, 0xDD});
    FormatConfig cfg;
    cfg.mode = OutputMode::NdefOnly;

    const auto fields = buildOutputFields(card, "hello", "", cfg);
    TEST_ASSERT_EQUAL_UINT32(1, fields.size());
    TEST_ASSERT_EQUAL_STRING("hello", fields[0].c_str());
}

/// NDEF_ONLY: NDEF が取得できなければ UID にフォールバックする
void test_fields_ndef_only_falls_back_to_uid(void)
{
    const auto card = makeCard({0xA3, 0x7A, 0x1B, 0xDD});
    FormatConfig cfg;
    cfg.mode = OutputMode::NdefOnly;

    const auto fields = buildOutputFields(card, "", "", cfg);
    TEST_ASSERT_EQUAL_UINT32(1, fields.size());
    TEST_ASSERT_EQUAL_STRING("A37A1BDD", fields[0].c_str());
}

/// UID_AND_NDEF: 2 つのフィールドを返す。区切りは含めない
void test_fields_uid_and_ndef(void)
{
    const auto card = makeCard({0xA3, 0x7A, 0x1B, 0xDD});
    FormatConfig cfg;
    cfg.mode = OutputMode::UidAndNdef;

    const auto fields = buildOutputFields(card, "hello", "", cfg);
    TEST_ASSERT_EQUAL_UINT32(2, fields.size());
    TEST_ASSERT_EQUAL_STRING("A37A1BDD", fields[0].c_str());
    TEST_ASSERT_EQUAL_STRING("hello", fields[1].c_str());
}

/// UID_AND_NDEF: NDEF が無ければ UID だけの 1 フィールドになる
void test_fields_uid_and_ndef_without_ndef(void)
{
    const auto card = makeCard({0xA3, 0x7A, 0x1B, 0xDD});
    FormatConfig cfg;
    cfg.mode = OutputMode::UidAndNdef;

    const auto fields = buildOutputFields(card, "", "", cfg);
    TEST_ASSERT_EQUAL_UINT32(1, fields.size());
    TEST_ASSERT_EQUAL_STRING("A37A1BDD", fields[0].c_str());
}

/// UID の書式設定はフィールド組み立てにも反映される
void test_fields_apply_uid_format(void)
{
    const auto card = makeCard({0xA3, 0x7A, 0x1B, 0xDD});
    FormatConfig cfg;
    cfg.mode          = OutputMode::UidOnly;
    cfg.uid_case      = UidCase::Lower;
    cfg.uid_separator = UidSeparator::Hyphen;

    const auto fields = buildOutputFields(card, "", "", cfg);
    TEST_ASSERT_EQUAL_UINT32(1, fields.size());
    TEST_ASSERT_EQUAL_STRING("a3-7a-1b-dd", fields[0].c_str());
}

/// 異常系: UID が無く NDEF も無ければ、出力すべきフィールドは無い
void test_fields_empty_when_nothing_to_output(void)
{
    CardInfo card;  // uid_size = 0
    FormatConfig cfg;
    cfg.mode = OutputMode::UidOnly;

    const auto fields = buildOutputFields(card, "", "", cfg);
    TEST_ASSERT_EQUAL_UINT32(0, fields.size());
}

/// 異常系: UID が無くても NDEF があれば出力する
void test_fields_ndef_without_uid(void)
{
    CardInfo card;  // uid_size = 0
    FormatConfig cfg;
    cfg.mode = OutputMode::UidAndNdef;

    const auto fields = buildOutputFields(card, "hello", "", cfg);
    TEST_ASSERT_EQUAL_UINT32(1, fields.size());
    TEST_ASSERT_EQUAL_STRING("hello", fields[0].c_str());
}

// --- F-12 COMMAND モード ----------------------------------------------------

/// COMMAND: 対応表の文字列を出力する
void test_fields_command_mode(void)
{
    const auto card = makeCard({0xA3, 0x7A, 0x1B, 0xDD});
    FormatConfig cfg;
    cfg.mode = OutputMode::Command;

    const auto fields = buildOutputFields(card, "ndef", "mapped text", cfg);
    TEST_ASSERT_EQUAL_UINT32(1, fields.size());
    TEST_ASSERT_EQUAL_STRING("mapped text", fields[0].c_str());
}

/// COMMAND: 未登録のカードは UID にフォールバックする
void test_fields_command_mode_falls_back_to_uid(void)
{
    const auto card = makeCard({0xA3, 0x7A, 0x1B, 0xDD});
    FormatConfig cfg;
    cfg.mode = OutputMode::Command;

    const auto fields = buildOutputFields(card, "ndef", "", cfg);
    TEST_ASSERT_EQUAL_UINT32(1, fields.size());
    TEST_ASSERT_EQUAL_STRING("A37A1BDD", fields[0].c_str());
}

/// COMMAND: UID も対応表も無ければ出力しない
void test_fields_command_mode_without_anything(void)
{
    CardInfo card;  // uid_size = 0
    FormatConfig cfg;
    cfg.mode = OutputMode::Command;

    TEST_ASSERT_EQUAL_UINT32(0, buildOutputFields(card, "", "", cfg).size());
}

/// 他のモードでは対応表の文字列を使わない
void test_other_modes_ignore_command_text(void)
{
    const auto card = makeCard({0xA3, 0x7A, 0x1B, 0xDD});
    FormatConfig cfg;

    cfg.mode          = OutputMode::UidOnly;
    const auto uid_only = buildOutputFields(card, "", "mapped text", cfg);
    TEST_ASSERT_EQUAL_UINT32(1, uid_only.size());
    TEST_ASSERT_EQUAL_STRING("A37A1BDD", uid_only[0].c_str());

    cfg.mode           = OutputMode::NdefOnly;
    const auto ndef_only = buildOutputFields(card, "ndef", "mapped text", cfg);
    TEST_ASSERT_EQUAL_UINT32(1, ndef_only.size());
    TEST_ASSERT_EQUAL_STRING("ndef", ndef_only[0].c_str());
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_uid_upper_without_separator);
    RUN_TEST(test_uid_lower_case);
    RUN_TEST(test_uid_colon_separator);
    RUN_TEST(test_uid_hyphen_separator);
    RUN_TEST(test_uid_pads_leading_zero);
    RUN_TEST(test_uid_7bytes);
    RUN_TEST(test_uid_10bytes);
    RUN_TEST(test_uid_empty_when_size_zero);
    RUN_TEST(test_fields_uid_only_ignores_ndef);
    RUN_TEST(test_fields_ndef_only);
    RUN_TEST(test_fields_ndef_only_falls_back_to_uid);
    RUN_TEST(test_fields_uid_and_ndef);
    RUN_TEST(test_fields_uid_and_ndef_without_ndef);
    RUN_TEST(test_fields_apply_uid_format);
    RUN_TEST(test_fields_empty_when_nothing_to_output);
    RUN_TEST(test_fields_ndef_without_uid);
    RUN_TEST(test_fields_command_mode);
    RUN_TEST(test_fields_command_mode_falls_back_to_uid);
    RUN_TEST(test_fields_command_mode_without_anything);
    RUN_TEST(test_other_modes_ignore_command_text);
    return UNITY_END();
}
