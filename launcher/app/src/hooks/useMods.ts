import { useMutation, useQuery, useQueryClient } from "@tanstack/react-query";
import { toast } from "sonner";
import { api, queryKeys } from "@/lib";
import type { Mod } from "@/lib";
import { i18n } from "@/i18n";

type ModList = { mods: Mod[]; dir: string };

/** The mods folder's mods in load order (mods.json), and saving a new order or selection. */
export function useMods() {
  const client = useQueryClient();
  const query = useQuery({ queryKey: queryKeys.mods, queryFn: api.listMods });
  const save = useMutation({
    mutationFn: (mods: Mod[]) => api.saveMods(mods.map((m) => m.name), mods.filter((m) => !m.enabled).map((m) => m.name)),
    onMutate: async (mods: Mod[]) => {
      await client.cancelQueries({ queryKey: queryKeys.mods });
      const previous = client.getQueryData<ModList>(queryKeys.mods);
      client.setQueryData<ModList>(queryKeys.mods, (list) => list && { ...list, mods });
      return { previous };
    },
    onError: (error, _mods, context) => {
      client.setQueryData(queryKeys.mods, context?.previous);
      toast.error(i18n.t("mods.saveFailed"), { description: String(error) });
    },
  });
  return { ...query, save: save.mutate };
}
