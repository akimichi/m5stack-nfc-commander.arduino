/*
 * command_map の実装 (仕様書 F-12)
 */
#include "command_map.h"

#include <cctype>

namespace nfccmd {

namespace {

/// 前後の空白を取り除く
std::string trim(const std::string& s)
{
    const auto is_space = [](const unsigned char c) { return std::isspace(c) != 0; };

    size_t first = 0;
    while (first < s.size() && is_space(static_cast<unsigned char>(s[first]))) {
        ++first;
    }
    size_t last = s.size();
    while (last > first && is_space(static_cast<unsigned char>(s[last - 1]))) {
        --last;
    }
    return s.substr(first, last - first);
}

/// 16 進数字なら 0〜15 を、そうでなければ -1 を返す
int hexValue(const char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/// NFC-A の UID 長として妥当か
bool isValidUidSize(const size_t size)
{
    return size == 4 || size == 7 || size == 10;
}

/*!
  @brief UID の 16 進表記を解析する
  @param text UID 表記。':' と '-' は区切りとして無視する
  @param[out] uid 解析結果
  @param[out] size バイト数
  @return 解析できたら true
 */
bool parseUid(const std::string& text, uint8_t (&uid)[10], uint8_t& size)
{
    uint8_t count   = 0;
    int high_nibble = -1;

    for (const char c : text) {
        if (c == ':' || c == '-') {
            continue;
        }
        const int value = hexValue(c);
        if (value < 0) {
            return false;
        }
        if (high_nibble < 0) {
            high_nibble = value;
            continue;
        }
        if (count >= 10) {
            return false;  // UID として長すぎる
        }
        uid[count++] = static_cast<uint8_t>((high_nibble << 4) | value);
        high_nibble  = -1;
    }

    // 桁数が奇数なら未処理のニブルが残る
    if (high_nibble >= 0) {
        return false;
    }
    if (!isValidUidSize(count)) {
        return false;
    }

    size = count;
    return true;
}

}  // namespace

bool CommandMap::addFromCsvLine(const std::string& line)
{
    const std::string trimmed = trim(line);
    if (trimmed.empty() || trimmed[0] == '#') {
        return false;
    }

    const auto comma = trimmed.find(',');
    if (comma == std::string::npos) {
        return false;
    }

    uint8_t uid[10]{};
    uint8_t uid_size{};
    if (!parseUid(trim(trimmed.substr(0, comma)), uid, uid_size)) {
        return false;
    }

    // 最初のカンマより後ろすべてを値とする。値にカンマを含められる
    const std::string text = trim(trimmed.substr(comma + 1));
    if (text.empty()) {
        return false;
    }

    // 同じ UID が既にあれば置き換える (後の行を優先する)
    for (auto& entry : entries_) {
        if (entry.uid_size == uid_size && std::memcmp(entry.uid, uid, uid_size) == 0) {
            entry.text = text;
            return true;
        }
    }

    if (entries_.size() >= kMaxCommandEntries) {
        return false;
    }

    Entry entry;
    std::memcpy(entry.uid, uid, sizeof(entry.uid));
    entry.uid_size = uid_size;
    entry.text     = text;
    entries_.push_back(entry);
    return true;
}

const std::string* CommandMap::find(const CardInfo& card) const
{
    if (!card.valid()) {
        return nullptr;
    }
    for (const auto& entry : entries_) {
        if (entry.uid_size == card.uid_size && std::memcmp(entry.uid, card.uid, card.uid_size) == 0) {
            return &entry.text;
        }
    }
    return nullptr;
}

}  // namespace nfccmd
