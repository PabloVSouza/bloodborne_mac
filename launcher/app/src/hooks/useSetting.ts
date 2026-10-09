import { useMutation, useQueryClient } from "@tanstack/react-query";
import { toast } from "sonner";
import { api, queryKeys } from "@/lib";
import type { Config, Settings } from "@/lib";
import { i18n } from "@/i18n";

type Change = { [K in keyof Settings]: { key: K; value: Settings[K] } }[keyof Settings];

/** Saves one launcher.json setting, shown at once (optimistic) and rolled back on failure. */
export function useSetting() {
  const client = useQueryClient();
  const mutation = useMutation({
    mutationFn: ({ key, value }: Change) => api.saveSettings({ [key]: value }),
    onMutate: async ({ key, value }: Change) => {
      await client.cancelQueries({ queryKey: queryKeys.config });
      const previous = client.getQueryData<Config>(queryKeys.config);
      client.setQueryData<Config>(queryKeys.config, (c) => c && { ...c, settings: { ...c.settings, [key]: value } });
      return { previous };
    },
    onError: (error, _change, context) => {
      client.setQueryData(queryKeys.config, context?.previous);
      toast.error(i18n.t("common.saveFailed"), { description: String(error) });
    },
    onSuccess: (_data, { key }) => {
      // The folders decide the mod and patch lists and the paths shown.
      if (key === "mods_dir" || key === "patches_dir" || key === "user_dir") {
        client.invalidateQueries({ queryKey: queryKeys.config });
        client.invalidateQueries({ queryKey: key === "mods_dir" ? queryKeys.mods : queryKeys.patches });
      }
    },
  });
  return <K extends keyof Settings>(key: K, value: Settings[K]) => mutation.mutate({ key, value } as Change);
}
