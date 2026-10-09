import { useQuery } from "@tanstack/react-query";
import { api, queryKeys } from "@/lib";

/** Connected controllers (SDL GUID and name); refetch to pick up new ones. */
export function useGamepads() {
  return useQuery({ queryKey: queryKeys.gamepads, queryFn: api.listGamepads });
}
