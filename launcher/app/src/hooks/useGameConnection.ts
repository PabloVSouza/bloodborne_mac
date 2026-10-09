import { useEffect } from "react";
import { useConfig } from "@/hooks/useConfig";
import { useGameStore } from "@/stores";

/** Feeds the game store with the backend's events once the config is loaded (it says whether a
 *  game started earlier is still running). */
export function useGameConnection() {
  const { data } = useConfig();
  const connect = useGameStore((s) => s.connect);
  const loaded = data !== undefined;
  const running = data?.running ?? false;
  useEffect(() => {
    if (!loaded) return;
    return connect(running);
    // Connected once: `running` is only the state at load time.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [loaded, connect]);
}
