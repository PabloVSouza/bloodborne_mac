import { useConfig } from "@/hooks/useConfig";
import type { Ini, Settings } from "@/lib";

/** The loaded settings and bbport.ini values (pages render once the config is loaded). */
export function useSettingsData(): { settings: Settings; ini: Ini } {
  const { data } = useConfig();
  return { settings: data?.settings ?? ({} as Settings), ini: data?.ini ?? {} };
}
