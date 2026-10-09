import { useEffect } from "react";
import { useConfig } from "@/hooks/useConfig";
import { i18n, resolveLanguage } from "@/i18n";

/** Applies the launcher language of launcher.json ("" = the system's). */
export function useLanguageSync() {
  const { data } = useConfig();
  const wanted = data?.settings.ui_language;
  useEffect(() => {
    if (wanted === undefined) return;
    const language = resolveLanguage(wanted);
    if (i18n.language !== language) i18n.changeLanguage(language);
    document.documentElement.lang = language;
  }, [wanted]);
}
