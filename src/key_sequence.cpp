/*
 * key_sequence の実装 (仕様書 F-13)
 */
#include "key_sequence.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace nfccmd {

namespace {

/// 特殊キー名と HID Usage ID の対応 (F-13)
struct SpecialKey {
    const char* name;
    uint8_t keycode;
};

constexpr SpecialKey kSpecialKeys[] = {
    // 編集
    {"ENTER", 0x28},
    {"ESC", 0x29},
    {"BS", 0x2A},
    {"TAB", 0x2B},
    {"SPACE", 0x2C},
    {"INS", 0x49},
    {"DEL", 0x4C},
    // 移動
    {"HOME", 0x4A},
    {"PGUP", 0x4B},
    {"END", 0x4D},
    {"PGDN", 0x4E},
    {"RIGHT", 0x4F},
    {"LEFT", 0x50},
    {"DOWN", 0x51},
    {"UP", 0x52},
    // 機能キー。F1〜F12 は 0x3A から連番だが、表に並べたほうが誤りに気づきやすい
    {"F1", 0x3A},
    {"F2", 0x3B},
    {"F3", 0x3C},
    {"F4", 0x3D},
    {"F5", 0x3E},
    {"F6", 0x3F},
    {"F7", 0x40},
    {"F8", 0x41},
    {"F9", 0x42},
    {"F10", 0x43},
    {"F11", 0x44},
    {"F12", 0x45},
};

/// 修飾キー名とビット (F-13)
struct ModifierName {
    const char* name;
    uint8_t bit;
};

constexpr ModifierName kModifiers[] = {
    {"CTRL", kModCtrl},
    {"SHIFT", kModShift},
    {"ALT", kModAlt},
    {"GUI", kModGui},
};

char toUpperAscii(const char c)
{
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
}

/// トークン名の比較。大文字小文字を区別しない (F-13)
bool equalsIgnoreCase(const std::string& s, const char* name)
{
    size_t i = 0;
    for (; i < s.size(); ++i) {
        if (name[i] == '\0' || toUpperAscii(s[i]) != name[i]) {
            return false;
        }
    }
    return name[i] == '\0';
}

/// 特殊キー名を引く。見つからなければ 0
uint8_t findSpecialKey(const std::string& name)
{
    for (const auto& key : kSpecialKeys) {
        if (equalsIgnoreCase(name, key.name)) {
            return key.keycode;
        }
    }
    return 0;
}

/// 修飾キー名を引く。見つからなければ 0
uint8_t findModifier(const std::string& name)
{
    for (const auto& mod : kModifiers) {
        if (equalsIgnoreCase(name, mod.name)) {
            return mod.bit;
        }
    }
    return 0;
}

bool isPrintableAscii(const char c)
{
    const unsigned char u = static_cast<unsigned char>(c);
    return u >= 0x20 && u <= 0x7E;
}

/*!
  @brief 修飾キーの並び ("CTRL+SHIFT" など) を解析する
  @param text 修飾キー部分。空文字列なら修飾キー無しとして成功する
  @param[out] modifiers ビットマスク
  @return 解析できたら true。未知の名前・空の要素・重複があれば false
 */
bool parseModifiers(const std::string& text, uint8_t& modifiers)
{
    modifiers = 0;
    if (text.empty()) {
        return true;
    }

    size_t begin = 0;
    while (true) {
        const size_t plus  = text.find('+', begin);
        const std::string name = text.substr(begin, plus == std::string::npos ? std::string::npos : plus - begin);
        if (name.empty()) {
            return false;  // "CTRL++SHIFT" のように区切りが連続している
        }
        const uint8_t bit = findModifier(name);
        if (bit == 0) {
            return false;  // 未知の修飾キー名
        }
        if ((modifiers & bit) != 0) {
            return false;  // 同じ修飾キーを 2 回書いている
        }
        modifiers |= bit;

        if (plus == std::string::npos) {
            return true;
        }
        begin = plus + 1;
    }
}

/*!
  @brief トークンの中身を 1 つのキーストロークへ解析する
  @param token `{` と `}` の間の文字列
  @param[out] item 解析結果 (kind は Chord になる)
  @return 解析できたら true
 */
bool parseToken(const std::string& token, SeqItem& item)
{
    if (token.empty()) {
        return false;
    }

    // 主キーが '+' のときは "CTRL++" となり、区切りの '+' と区別できない。
    // 末尾から順に取り除いて先に確定させる
    std::string body     = token;
    std::string main_key = {};
    if (body.back() == '+') {
        main_key = "+";
        body.pop_back();
        if (!body.empty()) {
            if (body.back() != '+') {
                return false;  // "CTRL+" のように主キーを欠いている
            }
            body.pop_back();
        }
    } else {
        const size_t plus = body.rfind('+');
        if (plus == std::string::npos) {
            main_key = body;
            body.clear();
        } else {
            main_key = body.substr(plus + 1);
            body     = body.substr(0, plus);
        }
        if (main_key.empty()) {
            return false;
        }
    }

    uint8_t modifiers = 0;
    if (!parseModifiers(body, modifiers)) {
        return false;
    }

    item.kind      = SeqItemKind::Chord;
    item.modifiers = modifiers;
    item.literal.clear();

    if (main_key.size() == 1 && isPrintableAscii(main_key[0])) {
        // 英字は小文字として扱い、Shift は付けない。
        // 大文字や Shift を伴う操作は {CTRL+SHIFT+C} のように明示させる (F-13)
        const char c = main_key[0];
        item.ascii   = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
        item.keycode = 0;
        return true;
    }

    const uint8_t keycode = findSpecialKey(main_key);
    if (keycode == 0) {
        return false;  // 未知のキー名
    }
    item.keycode = keycode;
    item.ascii   = '\0';
    return true;
}

/// literal 文字列を 1 要素として積む。空なら何もしない
void flushLiteral(std::string& pending, std::vector<SeqItem>& items)
{
    if (pending.empty()) {
        return;
    }
    SeqItem item;
    item.kind    = SeqItemKind::Literal;
    item.literal = pending;
    items.push_back(item);
    pending.clear();
}

}  // namespace

KeySequence parseKeySequence(const std::string& value)
{
    KeySequence result;
    std::string pending;

    size_t i = 0;
    while (i < value.size()) {
        if (value[i] != '{') {
            pending += value[i];
            ++i;
            continue;
        }

        // "{{" はリテラルの '{' 1 個 (F-13)
        if (i + 1 < value.size() && value[i + 1] == '{') {
            pending += '{';
            i += 2;
            continue;
        }

        // 中身は最大 kMaxTokenLength 文字。それを超えて '}' が現れなければ
        // トークンの開始とみなさない (F-13)
        const size_t search_end = std::min(value.size(), i + 1 + kMaxTokenLength + 1);
        size_t close            = std::string::npos;
        for (size_t j = i + 1; j < search_end; ++j) {
            if (value[j] == '}') {
                close = j;
                break;
            }
        }
        if (close == std::string::npos) {
            // 閉じない '{'。literal として残し、次の文字から解析を続ける
            result.has_error = true;
            pending += '{';
            ++i;
            continue;
        }

        SeqItem item;
        if (parseToken(value.substr(i + 1, close - i - 1), item)) {
            flushLiteral(pending, result.items);
            result.items.push_back(item);
        } else {
            // 解析できないトークンは '{' から '}' までをそのまま打鍵する (F-13)
            result.has_error = true;
            pending += value.substr(i, close - i + 1);
        }
        i = close + 1;
    }

    flushLiteral(pending, result.items);
    return result;
}

}  // namespace nfccmd
