/*
 * command_store の実装 (仕様書 F-12)
 */
#include "command_store.h"

#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>

#include <string>

namespace nfccmd {

namespace {

constexpr const char* kFilePath = "/nfc_commands.csv";
constexpr uint32_t kSpiClock    = 25000000;

/// 1 行分の文字列を対応表へ渡し、結果を数える
void feedLine(CommandMap& map, std::string& line, CommandStore::LoadResult& result)
{
    if (line.empty()) {
        return;
    }
    if (map.addFromCsvLine(line)) {
        ++result.loaded;
    } else {
        // コメント行と空行は数えない。書式の誤りだけを数える
        const auto first = line.find_first_not_of(" \t");
        if (first != std::string::npos && line[first] != '#') {
            ++result.skipped;
            M5_LOGW("command_store: skipped line: %s", line.c_str());
        }
    }
    line.clear();
}

}  // namespace

CommandStore::LoadResult CommandStore::load(CommandMap& map)
{
    LoadResult result;
    map.clear();

    const int cs   = M5.getPin(m5::pin_name_t::sd_spi_cs);
    const int sclk = M5.getPin(m5::pin_name_t::sd_spi_sclk);
    const int miso = M5.getPin(m5::pin_name_t::sd_spi_miso);
    const int mosi = M5.getPin(m5::pin_name_t::sd_spi_mosi);
    if (cs < 0 || sclk < 0 || miso < 0 || mosi < 0) {
        M5_LOGI("command_store: this board has no SD card slot");
        return result;
    }

    SPI.begin(sclk, miso, mosi, cs);
    if (!SD.begin(cs, SPI, kSpiClock)) {
        // カードが挿さっていないだけなのでエラーとはしない (F-12)
        M5_LOGI("command_store: SD card not available");
        return result;
    }
    result.sd_available = true;

    auto file = SD.open(kFilePath, FILE_READ);
    if (!file) {
        M5_LOGI("command_store: %s not found", kFilePath);
        SD.end();
        return result;
    }
    result.file_found = true;

    // 改行コードは CR / LF / CRLF のいずれでも扱えるようにする
    std::string line;
    while (file.available()) {
        const char c = static_cast<char>(file.read());
        if (c == '\n' || c == '\r') {
            feedLine(map, line, result);
            continue;
        }
        line += c;
    }
    feedLine(map, line, result);

    file.close();

    // SD のマウントだけ解除する。
    // CoreS3 は LCD (M5GFX) と SD が同じ SPI バスを共有しており、
    // M5GFX がバスを保持したまま SPI.end() を呼ぶと解放処理から戻らず停止する。
    // そのため SPI バスは開いたままにしておく
    SD.end();

    M5_LOGI("command_store: %u entries loaded, %u lines skipped", static_cast<unsigned>(result.loaded),
            static_cast<unsigned>(result.skipped));
    return result;
}

}  // namespace nfccmd
