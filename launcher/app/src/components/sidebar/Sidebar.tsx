import type { ReactNode } from "react";
import { useTranslation } from "react-i18next";
import { Gamepad2, House, MonitorCog, Package, ScrollText, Settings2, Swords, Wrench } from "lucide-react";
import { Moon } from "@/components/brand";
import { PlayButton } from "@/components/sidebar/PlayButton";
import { SidebarNavItem } from "@/components/sidebar/SidebarNavItem";
import { useConfig } from "@/hooks";
import { useGameStore, useUiStore } from "@/stores";
import type { Tab } from "@/stores";

const TABS: { id: Tab; icon: ReactNode }[] = [
  { id: "home", icon: <House /> },
  { id: "graphics", icon: <MonitorCog /> },
  { id: "controls", icon: <Gamepad2 /> },
  { id: "game", icon: <Swords /> },
  { id: "mods", icon: <Package /> },
  { id: "patches", icon: <Wrench /> },
  { id: "advanced", icon: <Settings2 /> },
  { id: "log", icon: <ScrollText /> },
];

export function Sidebar() {
  const { t } = useTranslation();
  const { tab, setTab } = useUiStore();
  const running = useGameStore((s) => s.running);
  const { data: config } = useConfig();
  return (
    <aside className="drag flex w-60 shrink-0 flex-col border-r border-sidebar-border bg-sidebar/80 px-3 pb-4 pt-11">
      <div className="mb-7 flex items-center gap-3 px-2">
        <Moon className="size-9" />
        <div>
          <div className="font-display text-lg font-semibold leading-tight tracking-wider">{t("app.name")}</div>
          <div className="text-[10px] uppercase tracking-[0.25em] text-muted-foreground">{t("app.tagline")}</div>
        </div>
      </div>
      <nav className="no-drag flex flex-col gap-0.5">
        {TABS.map((item) => (
          <SidebarNavItem
            key={item.id}
            label={t(`nav.${item.id}`)}
            icon={item.icon}
            active={tab === item.id}
            onClick={() => setTab(item.id)}
            badge={item.id === "log" && running ? <span className="block size-2 animate-pulse rounded-full bg-blood-400" /> : undefined}
          />
        ))}
      </nav>
      <div className="no-drag mt-auto">
        <PlayButton />
        <div className="mt-3 text-center text-[11px] text-muted-foreground">{t("app.version", { version: config?.version ?? "" })}</div>
      </div>
    </aside>
  );
}
