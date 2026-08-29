/*
 * key_sequence — 対応表の値をキーストロークの並びへ解析する (仕様書 F-13)
 *
 * `{CTRL+C}` のようなトークンを 1 つのキーストロークとして解釈し、
 * それ以外を literal な文字列として切り出す。
 *
 * 打鍵そのものは hid_output が行う。ここではレイアウトにも HID にも依存せず、
 * 「どの修飾キーと、どの主キーを押すか」までを決める。
 * ハードウェアに依存しない純粋ロジックとして実装する (§10.1)。
 *
 * 実装段階: S-11
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nfccmd {

/// キーシーケンスを解釈するかどうか (F-13)
enum class KeySeqMode : uint8_t {
    Off,  //!< 解釈せず literal として打鍵する (既定)
    On,   //!< `{CTRL+C}` などをキーストロークとして解釈する
};

/// 修飾キーのビットマスク (F-13)
enum ModifierBit : uint8_t {
    kModCtrl  = 0x01,
    kModShift = 0x02,
    kModAlt   = 0x04,
    kModGui   = 0x08,
};

/// トークンの中身 (`{` と `}` の間) に許す最大文字数 (F-13)
constexpr size_t kMaxTokenLength = 32;

/// キーシーケンスの 1 要素の種別
enum class SeqItemKind : uint8_t {
    Literal,  //!< そのまま打鍵する文字列
    Chord,    //!< 修飾キーを伴うキーストローク
};

/// キーシーケンスの 1 要素 (F-13)
struct SeqItem {
    SeqItemKind kind{SeqItemKind::Literal};

    //! kind == Literal のとき打鍵する文字列。空になることはない
    std::string literal{};

    //! kind == Chord のとき押す修飾キー (ModifierBit の論理和)
    uint8_t modifiers{0};

    //! kind == Chord で主キーが特殊キーのときの HID Usage ID。0 なら ascii を使う
    uint8_t keycode{0};

    //! kind == Chord で主キーが ASCII 文字のときの文字
    char ascii{'\0'};
};

/// 解析結果 (F-13)
struct KeySequence {
    std::vector<SeqItem> items{};

    //! 解析できないトークンがあったか。その部分は literal として items に入る
    bool has_error{false};

    /// キーストロークを 1 つでも含むか
    bool empty() const
    {
        return items.empty();
    }
};

/*!
  @brief 対応表の値を解析する (F-13)
  @param value 対応表に登録された文字列
  @return literal 部分とキーストロークの並び

  @note 対応する `}` が無い `{`、未知のキー名、主キーを欠くトークン、
        修飾キーの重複はいずれも解析失敗とし、その範囲を literal として
        そのまま残したうえで has_error を立てる。値を破棄しないのは、
        設定を ON にしただけで既存の対応表の値が黙って消えるのを避けるためである。
 */
KeySequence parseKeySequence(const std::string& value);

}  // namespace nfccmd
