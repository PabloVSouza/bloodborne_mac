// The game process: whether it runs, its output and how it ended. The backend's events
// ("game-output", "game-state") feed it; lines are batched a few times a second.
import { create } from "zustand";
import { toast } from "sonner";
import { api } from "@/lib";
import { i18n } from "@/i18n";

const MAX_LOG_LINES = 5000;

type GameStore = {
  running: boolean;
  /** Exit code of the last run (null: killed by a signal), undefined before any run ends. */
  lastExit: number | null | undefined;
  log: string[];
  startedAt: number | null;
  play: () => Promise<void>;
  stop: () => Promise<void>;
  clearLog: () => void;
  /** Subscribes to the backend's events; returns the unsubscribe function. */
  connect: (initiallyRunning: boolean) => () => void;
};

export const useGameStore = create<GameStore>((set) => ({
  running: false,
  lastExit: undefined,
  log: [],
  startedAt: null,
  play: async () => {
    set({ log: [], lastExit: undefined });
    try {
      await api.launch();
      set({ running: true, startedAt: Date.now() });
    } catch (error) {
      toast.error(i18n.t("play.failedToStart"), { description: String(error) });
    }
  },
  stop: async () => {
    await api.stop().catch((error) => toast.error(i18n.t("play.failedToStop"), { description: String(error) }));
  },
  clearLog: () => set({ log: [] }),
  connect: (initiallyRunning) => {
    set({ running: initiallyRunning });
    let pending: string[] = [];
    const flush = setInterval(() => {
      if (pending.length === 0) return;
      const lines = pending;
      pending = [];
      set((state) => {
        const log = state.log.concat(lines);
        return { log: log.length > MAX_LOG_LINES ? log.slice(log.length - MAX_LOG_LINES) : log };
      });
    }, 200);
    const output = api.onOutput((line) => pending.push(line));
    const state = api.onState(({ running, code }) => {
      set(running ? { running } : { running, lastExit: code ?? null, startedAt: null });
      if (!running && code !== 0 && code !== undefined && code !== null) {
        toast.error(i18n.t("play.exitedWithError"), { description: i18n.t("play.exitCode", { code }) });
      }
    });
    return () => {
      clearInterval(flush);
      output.then((unlisten) => unlisten());
      state.then((unlisten) => unlisten());
    };
  },
}));
