import type { ReactNode } from "react";

type Props = { icon: ReactNode; label: string; value: ReactNode; detail?: ReactNode; onClick?: () => void };

/** One fact of the summary; opens its settings page when clicked. */
export function SummaryTile({ icon, label, value, detail, onClick }: Props) {
  return (
    <button
      type="button"
      onClick={onClick}
      className="flex flex-col items-start gap-1 rounded-2xl border border-border bg-card/70 p-4 text-left transition hover:border-blood-600/40 hover:bg-card"
    >
      <span className="flex items-center gap-2 text-[11px] uppercase tracking-[0.18em] text-muted-foreground [&_svg]:size-3.5">
        {icon} {label}
      </span>
      <span className="text-base font-medium">{value}</span>
      {detail && <span className="text-xs text-muted-foreground">{detail}</span>}
    </button>
  );
}
