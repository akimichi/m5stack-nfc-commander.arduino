/*
 * ndef_parse — NDEF の TLV / Record を生バイト列から解析する (仕様書 F-03)
 *
 * MIFARE Classic は NFC Forum Tag Type ではないため、ライブラリの NDEFLayer を
 * 使えない。セクタから読み出した生データを自前で解析するために用いる。
 *
 * 解析結果は既存の parseTextRecordPayload() に渡して本文を得る。
 *   セクタデータ -> findNdefMessageTlv -> findTextRecordPayload -> parseTextRecordPayload
 *
 * ハードウェアに依存しない純粋ロジックとして実装する (§10.1)。
 *
 * 実装段階: S-7
 */
#pragma once

#include <cstdint>

namespace nfccmd {

/// バイト列の範囲。data は呼び出し側が保持するバッファを指す
struct ByteSpan {
    const uint8_t* data{nullptr};
    uint32_t size{0};

    bool valid() const
    {
        return data != nullptr;
    }
};

/*!
  @brief TLV 列から NDEF Message TLV (タグ 0x03) の値部分を探す
  @param data TLV 列の先頭
  @param len データ長
  @return 見つかれば NDEF Message の範囲。無ければ valid()==false
  @note NULL TLV (0x00) は読み飛ばし、Terminator TLV (0xFE) で終端とする。
        長さが 0xFF の場合は続く 2 バイトをビッグエンディアンの長さとして解釈する。
 */
ByteSpan findNdefMessageTlv(const uint8_t* data, uint32_t len);

/*!
  @brief NDEF Message から最初の Text レコードのペイロードを探す
  @param message NDEF Message の先頭
  @param len メッセージ長
  @return 見つかれば Text レコードのペイロード範囲。無ければ valid()==false
  @note TNF が Well Known (0x01) かつ Type が "T" のレコードを対象とする。
 */
ByteSpan findTextRecordPayload(const uint8_t* message, uint32_t len);

}  // namespace nfccmd
