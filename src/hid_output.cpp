/*
 * hid_output の実装 (仕様書 F-04 / F-05)
 */
#include "hid_output.h"

#include <Arduino.h>
#include <USB.h>
#include <USBHIDKeyboard.h>

namespace nfccmd {

namespace {

//! 修飾キーの HID Usage ID
constexpr uint8_t kHidLeftCtrl  = 0xE0;
constexpr uint8_t kHidLeftShift = 0xE1;
constexpr uint8_t kHidLeftAlt   = 0xE2;
constexpr uint8_t kHidLeftGui   = 0xE3;

/*
 * USBHIDKeyboard のコンストラクタが HID インターフェースを USB ディスクリプタへ
 * 登録する (hid.addDevice)。一方 Arduino core は ARDUINO_USB_CDC_ON_BOOT=1 のとき、
 * setup() より前の app_main で USB.begin() を呼んでしまう。
 *
 * このためインスタンスはグローバルに置き、静的初期化の段階で登録を済ませておく
 * 必要がある。setup() 内や関数内 static で生成すると登録が USB.begin() に間に合わず、
 * PC からキーボードとして認識されない。
 */
USBHIDKeyboard g_keyboard;

}  // namespace

void HidOutput::begin()
{
    g_keyboard.begin();

#if !ARDUINO_USB_CDC_ON_BOOT
    // CDC 有効時は Arduino core が app_main で USB.begin() を済ませている
    USB.begin();
#endif
}

bool HidOutput::isConnected() const
{
    // USBClass::operator bool() は _started && マウント済みを返す
    return static_cast<bool>(USB);
}

void HidOutput::writeKey(const uint8_t key)
{
    g_keyboard.write(key);
    if (cfg_.key_delay_ms != 0) {
        delay(cfg_.key_delay_ms);
    }
}

void HidOutput::writeChar(const char c)
{
    if (cfg_.layout == KeyboardLayout::JIS) {
        // USBHIDKeyboard::write() は US 配列前提の ASCII 変換表を使うため、
        // JIS では対応表から求めた生の HID キーコードを直接送る (F-05)
        const auto stroke = asciiToJisKeyStroke(c);
        if (!stroke.valid()) {
            return;
        }
        if (stroke.shift) {
            g_keyboard.pressRaw(kHidLeftShift);
        }
        g_keyboard.pressRaw(stroke.keycode);
        g_keyboard.releaseAll();
    } else {
        g_keyboard.write(static_cast<uint8_t>(c));
    }

    if (cfg_.key_delay_ms != 0) {
        delay(cfg_.key_delay_ms);
    }
}

HidSendResult HidOutput::send(const std::vector<std::string>& fields)
{
    HidSendResult result;

    // 打鍵できるフィールドだけを残す。空になったフィールドは区切りごと送らない
    std::vector<std::string> sanitized;
    sanitized.reserve(fields.size());
    for (const auto& field : fields) {
        const auto s = sanitizeForHid(field, cfg_.non_ascii);
        result.dropped_bytes = static_cast<uint16_t>(result.dropped_bytes + s.dropped_bytes);
        if (!s.text.empty()) {
            sanitized.push_back(s.text);
        }
    }

    if (sanitized.empty()) {
        return result;
    }
    if (!isConnected()) {
        result.not_connected = true;
        return result;
    }

    // 区切りキーと終端キー (Tab / Enter / Space / ',') は
    // US と JIS でキー位置が同じであるため、レイアウトによらず writeKey で送る
    bool first = true;
    for (const auto& field : sanitized) {
        if (!first) {
            switch (cfg_.field_separator) {
                case FieldSeparator::Space:
                    writeKey(' ');
                    break;
                case FieldSeparator::Comma:
                    writeKey(',');
                    break;
                case FieldSeparator::Tab:
                default:
                    writeKey(KEY_TAB);
                    break;
            }
        }
        first = false;

        for (const char c : field) {
            writeChar(c);
        }
    }

    writeTerminator();

    result.sent = true;
    return result;
}

void HidOutput::writeTerminator()
{
    switch (cfg_.terminator) {
        case TerminatorKey::Enter:
            writeKey(KEY_RETURN);
            break;
        case TerminatorKey::Tab:
            writeKey(KEY_TAB);
            break;
        case TerminatorKey::None:
        default:
            break;
    }
}

void HidOutput::writeChord(const SeqItem& item)
{
    // 修飾キーは HID Usage ID で押す。レイアウトによらず位置は同じである
    if ((item.modifiers & kModCtrl) != 0) {
        g_keyboard.pressRaw(kHidLeftCtrl);
    }
    if ((item.modifiers & kModShift) != 0) {
        g_keyboard.pressRaw(kHidLeftShift);
    }
    if ((item.modifiers & kModAlt) != 0) {
        g_keyboard.pressRaw(kHidLeftAlt);
    }
    if ((item.modifiers & kModGui) != 0) {
        g_keyboard.pressRaw(kHidLeftGui);
    }

    if (item.keycode != 0) {
        // 特殊キーの Usage ID も US / JIS で共通である
        g_keyboard.pressRaw(item.keycode);
    } else if (cfg_.layout == KeyboardLayout::JIS) {
        const auto stroke = asciiToJisKeyStroke(item.ascii);
        if (stroke.valid()) {
            if (stroke.shift) {
                g_keyboard.pressRaw(kHidLeftShift);
            }
            g_keyboard.pressRaw(stroke.keycode);
        }
    } else {
        // US 配列では USBHIDKeyboard の ASCII 変換表に任せる。
        // Shift が要る記号は press() が自分で付ける
        g_keyboard.press(static_cast<uint8_t>(item.ascii));
    }

    g_keyboard.releaseAll();

    if (cfg_.key_delay_ms != 0) {
        delay(cfg_.key_delay_ms);
    }
}

HidSendResult HidOutput::sendSequence(const std::string& value)
{
    HidSendResult result;

    const auto seq     = parseKeySequence(value);
    result.parse_error = seq.has_error;

    // literal 部分にだけサニタイズを適用する (F-13)。
    // トークンをまたいで連続した範囲外バイトが 1 個の '?' にまとまらないよう、
    // literal は要素ごとに処理する
    std::vector<SeqItem> items;
    items.reserve(seq.items.size());
    for (const auto& item : seq.items) {
        if (item.kind == SeqItemKind::Chord) {
            items.push_back(item);
            continue;
        }
        const auto s         = sanitizeForHid(item.literal, cfg_.non_ascii);
        result.dropped_bytes = static_cast<uint16_t>(result.dropped_bytes + s.dropped_bytes);
        if (s.text.empty()) {
            continue;
        }
        SeqItem literal = item;
        literal.literal = s.text;
        items.push_back(literal);
    }

    // キーストロークを 1 つも送らないなら終端キーも送らない (F-05 / F-13)
    if (items.empty()) {
        return result;
    }
    if (!isConnected()) {
        result.not_connected = true;
        return result;
    }

    for (const auto& item : items) {
        if (item.kind == SeqItemKind::Chord) {
            writeChord(item);
            continue;
        }
        for (const char c : item.literal) {
            writeChar(c);
        }
    }

    writeTerminator();

    result.sent = true;
    return result;
}

}  // namespace nfccmd
