// SPDX-License-Identifier: GPL-2.0-or-later
// bbport: the in-game menu's texts in the launcher's language (gpu/shim/locales/*.json, the
// launcher's languages; English for a missing text).
#pragma once

namespace BbText {

/// The menu language: BB_UI_LANGUAGE (the launcher's, e.g. "pt-BR"), else the system's
/// (LC_ALL, LC_MESSAGES, LANG, LANGUAGE), matched to a locale; "en" otherwise.
const char* Language();

/// The text of a key in the menu language (English when missing there; the key itself when
/// missing in English too). The pointer stays valid for the whole run.
const char* Get(const char* key);

/// Whether the menu language needs a CJK font (Japanese, Korean, Chinese).
bool NeedsCjkFont();

} // namespace BbText
