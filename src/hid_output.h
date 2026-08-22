/*
 * hid_output — USB HID キーボードによる打鍵出力 (仕様書 F-05)
 *
 * 仕様書 §5.3 のモジュール分割における hid_output に相当する。
 * サニタイズ処理は hid_sanitize に分離してホスト側テストの対象としている。
 *
 * 実装段階: S-7 (US / JIS 配列対応)
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hid_layout.h"
#include "hid_sanitize.h"

namespace nfccmd {

/// フィールド間の区切り (F-04)
/// 文字列に埋め込まずキーとして送る。Tab (0x09) はサニタイズ範囲外のためである
enum class FieldSeparator : uint8_t {
    Tab,    //!< Tab キー (既定)
    Space,  //!< 空白
    Comma,  //!< ','
};

/// 出力文字列の末尾に送るキー (F-04)
enum class TerminatorKey : uint8_t {
    None,   //!< 送らない
    Enter,  //!< Enter (既定)
    Tab,    //!< Tab
};

struct HidSendResult {
    bool sent{false};           //!< 実際に打鍵したか
    uint16_t dropped_bytes{0};  //!< 打鍵できず取り除いたバイト数
};

class HidOutput {
public:
    struct Config {
        //! 1 文字ごとの待ち時間 [ms]。受信側の取りこぼしを防ぐため既定は 0 にしない
        uint16_t key_delay_ms{8};
        //! 打鍵できないバイトの扱い
        NonAsciiPolicy non_ascii{NonAsciiPolicy::Drop};
        //! 末尾に送るキー
        TerminatorKey terminator{TerminatorKey::Enter};
        //! フィールド間の区切り
        FieldSeparator field_separator{FieldSeparator::Tab};
        //! PC 側のキーボードレイアウト
        KeyboardLayout layout{KeyboardLayout::US};
    };

    /// HID キーボードを開始する
    void begin();

    void config(const Config& cfg)
    {
        cfg_ = cfg;
    }

    const Config& config() const
    {
        return cfg_;
    }

    /*!
      @brief PC と接続されているか (F-11)
      @note USB が開始済みかつホストにマウントされている状態を指す
     */
    bool isConnected() const;

    /*!
      @brief フィールドの並びを打鍵する
      @param fields 出力するフィールド。フィールド間には区切りキーを送る
      @note 打鍵できないバイトは設定に従って処理される。サニタイズの結果
            すべてのフィールドが空になった場合は、終端キーも送らず sent=false を返す。
            Enter だけが単独で送られて誤入力になるのを避けるためである。
     */
    HidSendResult send(const std::vector<std::string>& fields);

private:
    /// 1 キー送出し、設定された間隔だけ待つ
    void writeKey(uint8_t key);

    /// 1 文字をレイアウトに応じて打鍵する
    void writeChar(char c);

    Config cfg_{};
};

}  // namespace nfccmd
