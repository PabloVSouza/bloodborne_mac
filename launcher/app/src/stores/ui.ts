import { create } from "zustand";

export type Tab = "home" | "graphics" | "controls" | "game" | "mods" | "patches" | "advanced" | "log";

type UiStore = {
  tab: Tab;
  setTab: (tab: Tab) => void;
};

const TABS: Tab[] = ["home", "graphics", "controls", "game", "mods", "patches", "advanced", "log"];

/** The first tab: the URL's hash when it names one (#graphics; previews and development). */
function initialTab(): Tab {
  const hash = typeof location !== "undefined" ? location.hash.slice(1) : "";
  return (TABS as string[]).includes(hash) ? (hash as Tab) : "home";
}

export const useUiStore = create<UiStore>((set) => ({
  tab: initialTab(),
  setTab: (tab) => set({ tab }),
}));
