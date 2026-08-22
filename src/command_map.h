/*
 * command_map — カード UID と送出文字列の対応表 (仕様書 F-12)
 *
 * SD カード上の CSV を解析して保持する。ファイルの読み込みは command_store が
 * 担当し、こちらは 1 行ずつ受け取って解析するため、ハードウェアに依存しない (§10.1)。
 *
 * 実装段階: S-10
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "nfc_reader.h"

namespace nfccmd {

/// 対応表に登録できる件数の上限 (F-12)
constexpr size_t kMaxCommandEntries = 500;

class CommandMap {
public:
    /*!
      @brief CSV の 1 行を解析して登録する
      @param line CSV の 1 行 (改行を含まないこと)
      @return 登録したら true。コメント行・空行・不正な行なら false
      @note 同じ UID が既にあれば後の行で置き換える。
            不正な行はファイル全体を無効にせず、その行だけを読み飛ばす。
     */
    bool addFromCsvLine(const std::string& line);

    /*!
      @brief UID に対応する文字列を探す
      @return 見つかればその文字列へのポインタ。無ければ nullptr
     */
    const std::string* find(const CardInfo& card) const;

    size_t size() const
    {
        return entries_.size();
    }

    bool empty() const
    {
        return entries_.empty();
    }

    void clear()
    {
        entries_.clear();
    }

private:
    struct Entry {
        uint8_t uid[10]{};
        uint8_t uid_size{};
        std::string text{};
    };

    std::vector<Entry> entries_{};
};

}  // namespace nfccmd
