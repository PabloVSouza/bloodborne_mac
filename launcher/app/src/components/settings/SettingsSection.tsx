import type { ReactNode } from "react";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui";
import { cn } from "@/lib";

type Props = { title?: string; description?: ReactNode; children: ReactNode; className?: string };

/** A card of setting rows. */
export function SettingsSection({ title, description, children, className }: Props) {
  return (
    <Card className={cn("mb-5 gap-0 border-border bg-card/70 py-0 shadow-lg shadow-black/30", className)}>
      {(title || description) && (
        <CardHeader className="border-b border-border px-5 pb-3 pt-4">
          {title && (
            <CardTitle className="font-display text-xs font-semibold uppercase tracking-[0.2em] text-blood-400">
              {title}
            </CardTitle>
          )}
          {description && <CardDescription className="text-xs leading-relaxed">{description}</CardDescription>}
        </CardHeader>
      )}
      <CardContent className="divide-y divide-border px-0">{children}</CardContent>
    </Card>
  );
}
