/*
 * ndef_text — NDEF Text レコードのペイロード解析 (仕様書 F-03)
 *
 * ペイロードの構造は [status][言語コード][本文] である。
 *   status: bit7   = 符号化 (0: UTF-8 / 1: UTF-16)
 *           bit5-0 = 言語コードのバイト長
 *
 * ハードウェアに依存しない純粋ロジックとして実装する (§10.1)。
 *
 * 実装段階: S-7
 */
#pragma once

#include <cstdint>
#include <string>

namespace nfccmd {

/// NDEF から取り出した本文の読み出し上限 [バイト] (F-03)
constexpr uint32_t kMaxNdefTextBytes = 256;

struct NdefTextResult {
    std::string text{};      //!< 本文 (UTF-8)
    bool valid{false};       //!< Text レコードとして解釈できたか
    bool truncated{false};   //!< 上限を超えて切り捨てたか
};

/*!
  @brief NDEF Text レコードのペイロードから本文を取り出す
  @param payload ペイロード先頭
  @param len ペイロード長
  @param max_bytes 本文の読み出し上限
  @note UTF-16 で符号化された本文は HID キーボードで打鍵できないため、
        valid=false を返して「NDEFなし」として扱う。
 */
NdefTextResult parseTextRecordPayload(const uint8_t* payload, uint32_t len,
                                      uint32_t max_bytes = kMaxNdefTextBytes);

}  // namespace nfccmd
