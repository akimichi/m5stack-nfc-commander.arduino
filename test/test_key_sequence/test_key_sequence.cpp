/*
 * key_sequence のテスト (仕様書 F-13 / §10.1)
 */
#include <unity.h>

#include <string>

#include "key_sequence.h"

namespace {

using nfccmd::KeySequence;
using nfccmd::parseKeySequence;
using nfccmd::SeqItemKind;

/// 要素が literal であり、内容が一致することを確かめる
void assertLiteral(const KeySequence& seq, const size_t index, const char* expected)
{
    TEST_ASSERT_TRUE(index < seq.items.size());
    TEST_ASSERT_EQUAL(SeqItemKind::Literal, seq.items[index].kind);
    TEST_ASSERT_EQUAL_STRING(expected, seq.items[index].literal.c_str());
}

/// 要素が ASCII 主キーのキーストロークであることを確かめる
void assertAsciiChord(const KeySequence& seq, const size_t index, const uint8_t modifiers, const char ascii)
{
    TEST_ASSERT_TRUE(index < seq.items.size());
    TEST_ASSERT_EQUAL(SeqItemKind::Chord, seq.items[index].kind);
    TEST_ASSERT_EQUAL_UINT8(modifiers, seq.items[index].modifiers);
    TEST_ASSERT_EQUAL_UINT8(0, seq.items[index].keycode);
    TEST_ASSERT_EQUAL_INT8(ascii, seq.items[index].ascii);
}

/// 要素が特殊キーのキーストロークであることを確かめる
void assertSpecialChord(const KeySequence& seq, const size_t index, const uint8_t modifiers, const uint8_t keycode)
{
    TEST_ASSERT_TRUE(index < seq.items.size());
    TEST_ASSERT_EQUAL(SeqItemKind::Chord, seq.items[index].kind);
    TEST_ASSERT_EQUAL_UINT8(modifiers, seq.items[index].modifiers);
    TEST_ASSERT_EQUAL_UINT8(keycode, seq.items[index].keycode);
}

}  // namespace

void setUp(void)
{
}

void tearDown(void)
{
}

// --- literal のみ -----------------------------------------------------------

/// トークンを含まない文字列は 1 つの literal になる
void test_plain_text(void)
{
    const auto seq = parseKeySequence("Hello World");
    TEST_ASSERT_FALSE(seq.has_error);
    TEST_ASSERT_EQUAL_UINT32(1, seq.items.size());
    assertLiteral(seq, 0, "Hello World");
}

/// 空文字列は要素を持たない
void test_empty_value(void)
{
    const auto seq = parseKeySequence("");
    TEST_ASSERT_FALSE(seq.has_error);
    TEST_ASSERT_TRUE(seq.empty());
}

/// '}' は '{' の外では通常の文字である
void test_close_brace_is_literal(void)
{
    const auto seq = parseKeySequence("a}b");
    TEST_ASSERT_FALSE(seq.has_error);
    TEST_ASSERT_EQUAL_UINT32(1, seq.items.size());
    assertLiteral(seq, 0, "a}b");
}

// --- 修飾キー付きのキーストローク -------------------------------------------

/// {CTRL+C} は Ctrl と 'c' になる。英字は小文字として扱い Shift を付けない
void test_ctrl_c(void)
{
    const auto seq = parseKeySequence("{CTRL+C}");
    TEST_ASSERT_FALSE(seq.has_error);
    TEST_ASSERT_EQUAL_UINT32(1, seq.items.size());
    assertAsciiChord(seq, 0, nfccmd::kModCtrl, 'c');
}

/// 大文字を送るには SHIFT を明示する
void test_ctrl_shift_c(void)
{
    const auto seq = parseKeySequence("{CTRL+SHIFT+C}");
    TEST_ASSERT_FALSE(seq.has_error);
    assertAsciiChord(seq, 0, nfccmd::kModCtrl | nfccmd::kModShift, 'c');
}

/// トークン名は大文字小文字を区別しない
void test_case_insensitive_token(void)
{
    const auto seq = parseKeySequence("{ctrl+alt+del}");
    TEST_ASSERT_FALSE(seq.has_error);
    assertSpecialChord(seq, 0, nfccmd::kModCtrl | nfccmd::kModAlt, 0x4C);
}

/// 修飾キー 4 種をすべて指定できる
void test_all_modifiers(void)
{
    const auto seq = parseKeySequence("{CTRL+SHIFT+ALT+GUI+A}");
    TEST_ASSERT_FALSE(seq.has_error);
    assertAsciiChord(seq, 0, nfccmd::kModCtrl | nfccmd::kModShift | nfccmd::kModAlt | nfccmd::kModGui, 'a');
}

/// 修飾キーの無い特殊キー単独も書ける
void test_special_key_alone(void)
{
    const auto seq = parseKeySequence("{ENTER}");
    TEST_ASSERT_FALSE(seq.has_error);
    assertSpecialChord(seq, 0, 0, 0x28);
}

/// ファンクションキーを引ける
void test_function_key(void)
{
    const auto seq = parseKeySequence("{ALT+F4}");
    TEST_ASSERT_FALSE(seq.has_error);
    assertSpecialChord(seq, 0, nfccmd::kModAlt, 0x3D);
}

/// 主キーに '+' を書ける (Ctrl と '+' の同時押し)
void test_plus_as_main_key(void)
{
    const auto seq = parseKeySequence("{CTRL++}");
    TEST_ASSERT_FALSE(seq.has_error);
    assertAsciiChord(seq, 0, nfccmd::kModCtrl, '+');
}

/// 修飾キーの無い '+' 単独も主キーとして扱える
void test_plus_alone(void)
{
    const auto seq = parseKeySequence("{+}");
    TEST_ASSERT_FALSE(seq.has_error);
    assertAsciiChord(seq, 0, 0, '+');
}

// --- literal とキーストロークの混在 -----------------------------------------

/// literal 部分はトークンの前後で分かれる
void test_mixed_literal_and_chord(void)
{
    const auto seq = parseKeySequence("name{TAB}value{ENTER}");
    TEST_ASSERT_FALSE(seq.has_error);
    TEST_ASSERT_EQUAL_UINT32(4, seq.items.size());
    assertLiteral(seq, 0, "name");
    assertSpecialChord(seq, 1, 0, 0x2B);
    assertLiteral(seq, 2, "value");
    assertSpecialChord(seq, 3, 0, 0x28);
}

/// 連続するトークンはそれぞれ別の要素になる
void test_consecutive_chords(void)
{
    const auto seq = parseKeySequence("{CTRL+A}{CTRL+C}");
    TEST_ASSERT_FALSE(seq.has_error);
    TEST_ASSERT_EQUAL_UINT32(2, seq.items.size());
    assertAsciiChord(seq, 0, nfccmd::kModCtrl, 'a');
    assertAsciiChord(seq, 1, nfccmd::kModCtrl, 'c');
}

// --- エスケープ -------------------------------------------------------------

/// "{{" はリテラルの '{' 1 個になる
void test_escaped_brace(void)
{
    const auto seq = parseKeySequence("literal brace: {{");
    TEST_ASSERT_FALSE(seq.has_error);
    TEST_ASSERT_EQUAL_UINT32(1, seq.items.size());
    assertLiteral(seq, 0, "literal brace: {");
}

/// "{{" の直後のトークンは通常どおり解釈される
void test_escaped_brace_then_token(void)
{
    const auto seq = parseKeySequence("{{{ENTER}");
    TEST_ASSERT_FALSE(seq.has_error);
    TEST_ASSERT_EQUAL_UINT32(2, seq.items.size());
    assertLiteral(seq, 0, "{");
    assertSpecialChord(seq, 1, 0, 0x28);
}

// --- 解析できない場合は literal に落ちる ------------------------------------

/// 閉じない '{' は literal として残る
void test_unclosed_brace(void)
{
    const auto seq = parseKeySequence("a{CTRL+C");
    TEST_ASSERT_TRUE(seq.has_error);
    TEST_ASSERT_EQUAL_UINT32(1, seq.items.size());
    assertLiteral(seq, 0, "a{CTRL+C");
}

/// 未知のキー名は '{' から '}' までをそのまま打鍵する
void test_unknown_key_name(void)
{
    const auto seq = parseKeySequence("x{FOO}y");
    TEST_ASSERT_TRUE(seq.has_error);
    TEST_ASSERT_EQUAL_UINT32(1, seq.items.size());
    assertLiteral(seq, 0, "x{FOO}y");
}

/// 主キーを欠くトークンは不正である
void test_modifier_only_token(void)
{
    const auto seq = parseKeySequence("{CTRL}");
    TEST_ASSERT_TRUE(seq.has_error);
    assertLiteral(seq, 0, "{CTRL}");
}

/// 修飾キーの重複は不正である
void test_duplicated_modifier(void)
{
    const auto seq = parseKeySequence("{CTRL+CTRL+C}");
    TEST_ASSERT_TRUE(seq.has_error);
    assertLiteral(seq, 0, "{CTRL+CTRL+C}");
}

/// 未知の修飾キー名は不正である
void test_unknown_modifier(void)
{
    const auto seq = parseKeySequence("{META+C}");
    TEST_ASSERT_TRUE(seq.has_error);
    assertLiteral(seq, 0, "{META+C}");
}

/// 空のトークンは不正である
void test_empty_token(void)
{
    const auto seq = parseKeySequence("{}");
    TEST_ASSERT_TRUE(seq.has_error);
    assertLiteral(seq, 0, "{}");
}

/// 不正なトークンがあっても、後続の正しいトークンは解釈される
void test_error_does_not_stop_parsing(void)
{
    const auto seq = parseKeySequence("{FOO}{ENTER}");
    TEST_ASSERT_TRUE(seq.has_error);
    TEST_ASSERT_EQUAL_UINT32(2, seq.items.size());
    assertLiteral(seq, 0, "{FOO}");
    assertSpecialChord(seq, 1, 0, 0x28);
}

// --- トークン長の上限 -------------------------------------------------------

/// 中身が 32 文字までなら探索する (未知の名前なので literal に落ちる)
void test_token_at_length_limit(void)
{
    const std::string name(nfccmd::kMaxTokenLength, 'A');
    const auto seq = parseKeySequence("{" + name + "}");
    TEST_ASSERT_TRUE(seq.has_error);
    // '}' は見つかったので、'{' から '}' までがまとめて literal になる
    TEST_ASSERT_EQUAL_UINT32(1, seq.items.size());
    assertLiteral(seq, 0, ("{" + name + "}").c_str());
}

/// 32 文字を超えると '}' を探さず、'{' だけが literal になる
void test_token_over_length_limit(void)
{
    const std::string name(nfccmd::kMaxTokenLength + 1, 'A');
    const auto seq = parseKeySequence("{" + name + "}");
    TEST_ASSERT_TRUE(seq.has_error);
    TEST_ASSERT_EQUAL_UINT32(1, seq.items.size());
    // '{' はトークンの開始とみなされず、以降はすべて通常の文字として扱われる
    assertLiteral(seq, 0, ("{" + name + "}").c_str());
}

int main(int, char**)
{
    UNITY_BEGIN();

    RUN_TEST(test_plain_text);
    RUN_TEST(test_empty_value);
    RUN_TEST(test_close_brace_is_literal);

    RUN_TEST(test_ctrl_c);
    RUN_TEST(test_ctrl_shift_c);
    RUN_TEST(test_case_insensitive_token);
    RUN_TEST(test_all_modifiers);
    RUN_TEST(test_special_key_alone);
    RUN_TEST(test_function_key);
    RUN_TEST(test_plus_as_main_key);
    RUN_TEST(test_plus_alone);

    RUN_TEST(test_mixed_literal_and_chord);
    RUN_TEST(test_consecutive_chords);

    RUN_TEST(test_escaped_brace);
    RUN_TEST(test_escaped_brace_then_token);

    RUN_TEST(test_unclosed_brace);
    RUN_TEST(test_unknown_key_name);
    RUN_TEST(test_modifier_only_token);
    RUN_TEST(test_duplicated_modifier);
    RUN_TEST(test_unknown_modifier);
    RUN_TEST(test_empty_token);
    RUN_TEST(test_error_does_not_stop_parsing);

    RUN_TEST(test_token_at_length_limit);
    RUN_TEST(test_token_over_length_limit);

    return UNITY_END();
}
