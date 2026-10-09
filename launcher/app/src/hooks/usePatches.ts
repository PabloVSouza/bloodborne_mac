import { useMutation, useQuery, useQueryClient } from "@tanstack/react-query";
import { toast } from "sonner";
import { api, queryKeys } from "@/lib";
import type { Patch } from "@/lib";
import { i18n } from "@/i18n";

type PatchList = { patches: Patch[]; dir: string };

/** Third-party patches of the patches folder, and saving the switches (patches.json keeps only
 *  those that differ from each file's default). */
export function usePatches() {
  const client = useQueryClient();
  const query = useQuery({ queryKey: queryKeys.patches, queryFn: api.listPatches });
  const save = useMutation({
    mutationFn: (patches: Patch[]) =>
      api.savePatches(
        patches.map((p) => p.key),
        patches.filter((p) => p.enabled && !p.default).map((p) => p.key),
        patches.filter((p) => !p.enabled && p.default).map((p) => p.key),
      ),
    onMutate: async (patches: Patch[]) => {
      await client.cancelQueries({ queryKey: queryKeys.patches });
      const previous = client.getQueryData<PatchList>(queryKeys.patches);
      client.setQueryData<PatchList>(queryKeys.patches, (list) => list && { ...list, patches });
      return { previous };
    },
    onError: (error, _patches, context) => {
      client.setQueryData(queryKeys.patches, context?.previous);
      toast.error(i18n.t("patches.saveFailed"), { description: String(error) });
    },
  });
  return { ...query, save: save.mutate };
}
