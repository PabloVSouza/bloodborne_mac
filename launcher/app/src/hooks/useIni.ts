import { useMutation, useQueryClient } from "@tanstack/react-query";
import { toast } from "sonner";
import { api, queryKeys } from "@/lib";
import type { Config } from "@/lib";
import { i18n } from "@/i18n";

type Change = { key: string; value: string | null };

/** Saves one bbport.ini value (null removes the line: the default applies), optimistically. */
export function useIni() {
  const client = useQueryClient();
  const mutation = useMutation({
    mutationFn: ({ key, value }: Change) => api.saveIni({ [key]: value }),
    onMutate: async ({ key, value }: Change) => {
      await client.cancelQueries({ queryKey: queryKeys.config });
      const previous = client.getQueryData<Config>(queryKeys.config);
      client.setQueryData<Config>(queryKeys.config, (c) => {
        if (!c) return c;
        const ini = { ...c.ini };
        if (value === null) delete ini[key];
        else ini[key] = value;
        return { ...c, ini };
      });
      return { previous };
    },
    onError: (error, _change, context) => {
      client.setQueryData(queryKeys.config, context?.previous);
      toast.error(i18n.t("common.saveFailed"), { description: String(error) });
    },
  });
  return (key: string, value: string | null) => mutation.mutate({ key, value });
}
