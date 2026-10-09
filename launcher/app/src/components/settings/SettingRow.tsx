import type { ReactNode } from "react";
import { cn } from "@/lib";

type Props = { title: ReactNode; description?: ReactNode; children?: ReactNode; disabled?: boolean };

/** A setting: title and explanation on the left, its control on the right. */
export function SettingRow({ title, description, children, disabled }: Props) {
  return (
    <div className={cn("flex items-center justify-between gap-6 px-5 py-3.5", disabled && "pointer-events-none opacity-40")}>
      <div className="min-w-0">
        <div className="text-sm font-medium">{title}</div>
        {description && <div className="mt-0.5 text-xs leading-relaxed text-muted-foreground">{description}</div>}
      </div>
      <div className="flex shrink-0 items-center gap-2">{children}</div>
    </div>
  );
}
