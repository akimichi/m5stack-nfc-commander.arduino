/*
 * command_store — SD カードからの対応表の読み込み (仕様書 F-12)
 *
 * CSV の解析は command_map が担当し、こちらはファイルの読み出しに専念する。
 *
 * 実装段階: S-10
 */
#pragma once

#include <cstddef>

#include "command_map.h"

namespace nfccmd {

class CommandStore {
public:
    struct LoadResult {
        bool sd_available{false};  //!< SD カードを認識できたか
        bool file_found{false};    //!< 対応表のファイルがあったか
        size_t loaded{0};          //!< 登録した件数
        size_t skipped{0};         //!< 読み飛ばした行数 (コメント・空行を除く)
    };

    /*!
      @brief SD カードの対応表を読み込む
      @param[out] map 読み込んだ内容。呼び出し時に内容は破棄される
      @note SD カードやファイルが無くてもエラーとはしない。対応表が空になるだけで、
            その場合はすべてのカードが UID へフォールバックする (F-12)
     */
    LoadResult load(CommandMap& map);
};

}  // namespace nfccmd
