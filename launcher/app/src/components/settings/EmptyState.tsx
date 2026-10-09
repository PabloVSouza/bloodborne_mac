import type { ReactNode } from "react";

type Props = { icon: ReactNode; title: string; children?: ReactNode };

export function EmptyState({ icon, title, children }: Props) {
  return (
    <div className="flex flex-col items-center px-6 py-12 text-center">
      <div className="mb-3 text-muted-foreground/70">{icon}</div>
      <div className="font-display text-base">{title}</div>
      {children && <div className="mt-2 max-w-md text-xs leading-relaxed text-muted-foreground">{children}</div>}
    </div>
  );
}
