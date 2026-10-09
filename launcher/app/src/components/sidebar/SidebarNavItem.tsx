import type { ReactNode } from "react";
import { cn } from "@/lib";

type Props = { label: string; icon: ReactNode; active: boolean; onClick: () => void; badge?: ReactNode };

export function SidebarNavItem({ label, icon, active, onClick, badge }: Props) {
  return (
    <button
      type="button"
      onClick={onClick}
      className={cn(
        "flex items-center gap-3 rounded-lg px-3 py-2 text-sm transition",
        active
          ? "bg-sidebar-accent text-sidebar-accent-foreground ring-1 ring-blood-600/40"
          : "text-muted-foreground hover:bg-white/5 hover:text-foreground",
      )}
    >
      <span className={cn("[&_svg]:size-4", active && "text-blood-400")}>{icon}</span>
      {label}
      {badge && <span className="ml-auto">{badge}</span>}
    </button>
  );
}
