// Launcher texts (react-i18next). en.json is the reference: its keys are the typed keys
// (i18next.d.ts); other locales follow it, missing keys fall back to English.
import i18n from "i18next";
import { initReactI18next } from "react-i18next";
import en from "@/i18n/locales/en.json";
import ptBR from "@/i18n/locales/pt-BR.json";

export const LAUNCHER_LANGUAGES = [
  { code: "en", name: "English" },
  { code: "pt-BR", name: "Português (Brasil)" },
] as const;

/** A supported language for a code ("" = the system's), English otherwise. */
export function resolveLanguage(code: string): string {
  const wanted = code || navigator.language || "en";
  const exact = LAUNCHER_LANGUAGES.find((l) => l.code.toLowerCase() === wanted.toLowerCase());
  if (exact) return exact.code;
  const base = LAUNCHER_LANGUAGES.find((l) => l.code.split("-")[0] === wanted.split("-")[0]);
  return base?.code ?? "en";
}

i18n.use(initReactI18next).init({
  resources: { en: { translation: en }, "pt-BR": { translation: ptBR } },
  lng: resolveLanguage(""),
  fallbackLng: "en",
  interpolation: { escapeValue: false },
  returnNull: false,
});

export { i18n };
