import { useQuery } from "@tanstack/react-query";
import { api, queryKeys } from "@/lib";

/** The game folder's problem (no eboot.bin, wrong version, ...) or null when it can start.
 *  The check reads the executable: cached per folder. */
export function useGameCheck(dir: string | undefined) {
  return useQuery({
    queryKey: queryKeys.gameCheck(dir ?? ""),
    queryFn: () => api.checkGame(dir ?? ""),
    enabled: dir !== undefined,
  });
}
