/*
 * settings_store の実装 (仕様書 F-10)
 */
#include "settings_store.h"

#include <M5Unified.h>
#include <Preferences.h>

namespace nfccmd {

namespace {

constexpr const char* kNamespace = "nfccmd";

// 保存形式の版。将来項目を変えたときに、古い内容を既定値として扱えるようにする
constexpr uint8_t kFormatVersion = 1;

// NVS のキー名は 15 文字以内である必要がある
constexpr const char* kKeyVersion   = "ver";
constexpr const char* kKeyOutMode   = "out_mode";
constexpr const char* kKeyUidCase   = "uid_case";
constexpr const char* kKeyUidSep    = "uid_sep";
constexpr const char* kKeyFieldSep  = "field_sep";
constexpr const char* kKeyTermKey   = "term_key";
constexpr const char* kKeyKeyDelay  = "key_delay";
constexpr const char* kKeyLayout    = "layout";
constexpr const char* kKeyNonAscii  = "nonascii";
constexpr const char* kKeyDebounce  = "debounce";
constexpr const char* kKeyAbsentTh  = "absent_th";
constexpr const char* kKeyPollMs    = "poll_ms";

template <typename E>
uint8_t toU8(const E value)
{
    return static_cast<uint8_t>(value);
}

}  // namespace

bool SettingsStore::load(Settings& out)
{
    const Settings defaults;

    Preferences prefs;
    if (!prefs.begin(kNamespace, /*readOnly=*/true)) {
        // 名前空間が存在しない = 一度も保存していない
        out = defaults;
        return false;
    }

    const uint8_t version = prefs.getUChar(kKeyVersion, 0);
    if (version != kFormatVersion) {
        M5_LOGW("settings_store: format version mismatch (%u), using defaults", static_cast<unsigned>(version));
        prefs.end();
        out = defaults;
        return false;
    }

    Settings s;
    s.out_mode        = static_cast<OutputMode>(prefs.getUChar(kKeyOutMode, toU8(defaults.out_mode)));
    s.uid_case        = static_cast<UidCase>(prefs.getUChar(kKeyUidCase, toU8(defaults.uid_case)));
    s.uid_separator   = static_cast<UidSeparator>(prefs.getUChar(kKeyUidSep, toU8(defaults.uid_separator)));
    s.field_separator = static_cast<FieldSeparator>(prefs.getUChar(kKeyFieldSep, toU8(defaults.field_separator)));
    s.terminator      = static_cast<TerminatorKey>(prefs.getUChar(kKeyTermKey, toU8(defaults.terminator)));
    s.key_delay_ms    = prefs.getUShort(kKeyKeyDelay, defaults.key_delay_ms);
    s.layout          = static_cast<KeyboardLayout>(prefs.getUChar(kKeyLayout, toU8(defaults.layout)));
    s.non_ascii       = static_cast<NonAsciiPolicy>(prefs.getUChar(kKeyNonAscii, toU8(defaults.non_ascii)));
    s.debounce_ms     = prefs.getUShort(kKeyDebounce, defaults.debounce_ms);
    s.absent_threshold = prefs.getUChar(kKeyAbsentTh, defaults.absent_threshold);
    s.poll_ms          = prefs.getUShort(kKeyPollMs, defaults.poll_ms);
    prefs.end();

    // 壊れた値が混ざっていても起動できるようにする (F-10)
    s.clampToValidRange();
    out = s;
    return true;
}

bool SettingsStore::save(const Settings& settings)
{
    Preferences prefs;
    if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
        M5_LOGE("settings_store: failed to open NVS for write");
        return false;
    }

    prefs.putUChar(kKeyVersion, kFormatVersion);
    prefs.putUChar(kKeyOutMode, toU8(settings.out_mode));
    prefs.putUChar(kKeyUidCase, toU8(settings.uid_case));
    prefs.putUChar(kKeyUidSep, toU8(settings.uid_separator));
    prefs.putUChar(kKeyFieldSep, toU8(settings.field_separator));
    prefs.putUChar(kKeyTermKey, toU8(settings.terminator));
    prefs.putUShort(kKeyKeyDelay, settings.key_delay_ms);
    prefs.putUChar(kKeyLayout, toU8(settings.layout));
    prefs.putUChar(kKeyNonAscii, toU8(settings.non_ascii));
    prefs.putUShort(kKeyDebounce, settings.debounce_ms);
    prefs.putUChar(kKeyAbsentTh, settings.absent_threshold);
    prefs.putUShort(kKeyPollMs, settings.poll_ms);
    prefs.end();

    M5_LOGI("settings_store: saved");
    return true;
}

bool SettingsStore::clear()
{
    Preferences prefs;
    if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
        return false;
    }
    const bool ok = prefs.clear();
    prefs.end();
    return ok;
}

}  // namespace nfccmd
