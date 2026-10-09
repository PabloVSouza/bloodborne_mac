// SPDX-License-Identifier: GPL-2.0-or-later
#include "bbport_text.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>

namespace BbText {
namespace {

struct TextEntry {
    const char* language;
    const char* key;
    const char* text;
};
#include "bbport_text_data.inc"

std::string Lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = char(std::tolower(static_cast<unsigned char>(c == '_' ? '-' : c)));
    return out;
}

bool Has(std::string_view language) {
    for (const auto& e : kTexts) {
        if (language == e.language) return true;
    }
    return false;
}

/// A locale for a language code: exact, then the nearest regional variant (as the launcher:
/// Spanish outside Spain to Latin American, Chinese of HK/MO/TW/Hant to Traditional, Norwegian
/// to Bokmål, Portuguese to Brazilian), then the base language.
std::string Match(std::string_view code) {
    const std::string wanted = Lower(code.substr(0, code.find('.'))); // "pt_BR.UTF-8" -> "pt-br"
    if (wanted.empty() || wanted == "c" || wanted == "posix") return {};
    for (const auto& e : kTexts) {
        if (Lower(e.language) == wanted) return e.language;
    }
    const std::string base = wanted.substr(0, wanted.find('-'));
    const std::string region = wanted.size() > base.size() ? wanted.substr(base.size() + 1) : "";
    if (base == "es") return region.empty() || region == "es" ? "es-ES" : "es-419";
    if (base == "zh") {
        const bool traditional = region.find("hant") != std::string::npos || region == "tw" ||
                                 region == "hk" || region == "mo";
        return traditional ? "zh-TW" : "zh-CN";
    }
    if (base == "no" || base == "nn" || base == "nb") return "nb";
    if (base == "pt") return region == "pt" ? "pt-PT" : "pt-BR";
    for (const auto& e : kTexts) {
        const std::string_view language = e.language;
        if (language.substr(0, language.find('-')) == base) return std::string(language);
    }
    return {};
}

std::string Detect() {
    if (const char* ui = std::getenv("BB_UI_LANGUAGE"); ui && *ui) {
        if (std::string m = Match(ui); !m.empty() && Has(m)) return m;
    }
    for (const char* var : {"LC_ALL", "LC_MESSAGES", "LANG", "LANGUAGE"}) {
        if (const char* v = std::getenv(var); v && *v) {
            if (std::string m = Match(v); !m.empty() && Has(m)) return m;
        }
    }
    return "en";
}

struct Table {
    std::string language = Detect();
    std::unordered_map<std::string_view, const char*> texts;
    Table() {
        // English first, then the language's own texts over it.
        for (const char* wanted : {"en", language.c_str()}) {
            for (const auto& e : kTexts) {
                if (std::strcmp(e.language, wanted) == 0) texts[e.key] = e.text;
            }
        }
    }
};

const Table& Instance() {
    static const Table table;
    return table;
}

} // namespace

const char* Language() {
    return Instance().language.c_str();
}

const char* Get(const char* key) {
    const auto& texts = Instance().texts;
    const auto it = texts.find(key);
    return it != texts.end() ? it->second : key;
}

bool NeedsCjkFont() {
    const std::string_view language = Language();
    return language == "ja" || language == "ko" || language.substr(0, 2) == "zh";
}

} // namespace BbText
