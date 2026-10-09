import { QueryClient } from "@tanstack/react-query";

// Backend data is local and changes only through this app (or the game's menu, between
// starts): no polling, no refetch on focus.
export const queryClient = new QueryClient({
  defaultOptions: {
    queries: { staleTime: Infinity, refetchOnWindowFocus: false, retry: false },
  },
});

export const queryKeys = {
  config: ["config"] as const,
  gameCheck: (dir: string) => ["game-check", dir] as const,
  mods: ["mods"] as const,
  patches: ["patches"] as const,
  gamepads: ["gamepads"] as const,
};
