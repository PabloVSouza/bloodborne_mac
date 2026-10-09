// Launcher texts (react-i18next). en.json is the reference: its keys are the typed keys
// (i18next.d.ts); every other file in locales/ follows it (scripts/check_locales.py), and a
// missing key falls back to English. The languages are the game's own (dvdroot_ps4/msg).
import i18n from "i18next";
import { initReactI18next } from "react-i18next";

const files = import.meta.glob<Record<string, unknown>>("/src/i18n/locales/*.json", { eager: true, import: "default" });
const resources = Object.fromEntries(
  Object.entries(files).map(([path, translation]) => [path.replace(/^.*\/(.+)\.json$/, "$1"), { translation }]),
);

/** The launcher languages, by their own names (Advanced → Launcher language). */
export const LAUNCHER_LANGUAGES = [
  { code: "en", name: "English" },
  { code: "pt-BR", name: "Português (Brasil)" },
  { code: "pt-PT", name: "Português (Portugal)" },
  { code: "es-ES", name: "Español (España)" },
  { code: "es-419", name: "Español (Latinoamérica)" },
  { code: "fr", name: "Français" },
  { code: "de", name: "Deutsch" },
  { code: "it", name: "Italiano" },
  { code: "nl", name: "Nederlands" },
  { code: "pl", name: "Polski" },
  { code: "ru", name: "Русский" },
  { code: "tr", name: "Türkçe" },
  { code: "fi", name: "Suomi" },
  { code: "sv", name: "Svenska" },
  { code: "da", name: "Dansk" },
  { code: "nb", name: "Norsk" },
  { code: "ja", name: "日本語" },
  { code: "ko", name: "한국어" },
  { code: "zh-CN", name: "简体中文" },
  { code: "zh-TW", name: "繁體中文" },
] as const;

/** A supported language for a code ("" = the system's), English otherwise. Regional variants
 *  go to their nearest: Spanish outside Spain to Latin American Spanish, Chinese of Hong Kong,
 *  Macau and Taiwan (or Hant) to Traditional, Norwegian to Bokmål, Portuguese to Brazilian. */
export function resolveLanguage(code: string): string {
  const wanted = (code || navigator.language || "en").replace("_", "-");
  const lower = wanted.toLowerCase();
  const exact = LAUNCHER_LANGUAGES.find((l) => l.code.toLowerCase() === lower);
  if (exact) return exact.code;
  const [base, ...rest] = lower.split("-");
  const region = rest.join("-");
  if (base === "es") return region === "es" ? "es-ES" : region ? "es-419" : "es-ES";
  if (base === "zh") return /hant|tw|hk|mo/.test(region) ? "zh-TW" : "zh-CN";
  if (base === "no" || base === "nn" || base === "nb") return "nb";
  if (base === "pt") return region === "pt" ? "pt-PT" : "pt-BR";
  return LAUNCHER_LANGUAGES.find((l) => l.code.split("-")[0] === base)?.code ?? "en";
}

i18n.use(initReactI18next).init({
  resources,
  lng: resolveLanguage(""),
  fallbackLng: "en",
  interpolation: { escapeValue: false },
  returnNull: false,
});

export { i18n };
