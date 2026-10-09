import { useQuery } from "@tanstack/react-query";
import { api, queryKeys } from "@/lib";

/** launcher.json, bbport.ini and the data paths. */
export function useConfig() {
  return useQuery({ queryKey: queryKeys.config, queryFn: api.loadConfig });
}
