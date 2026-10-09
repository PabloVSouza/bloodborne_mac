import { useTranslation } from "react-i18next";
import { Switch } from "@/components/ui";
import type { Patch } from "@/lib";

type Props = { patch: Patch; onToggle: (enabled: boolean) => void };

/** A third-party patch: name, file and author, its note, on/off. */
export function PatchRow({ patch, onToggle }: Props) {
  const { t } = useTranslation();
  return (
    <div className="flex items-start gap-4 px-5 py-3.5">
      <div className="min-w-0 flex-1">
        <div className="text-sm font-medium">{patch.name}</div>
        <div className="mt-0.5 text-xs text-muted-foreground">
          <span className="font-mono">{patch.file}</span>
          {patch.author && <span> · {t("patches.by", { author: patch.author })}</span>}
        </div>
        {patch.note && <div className="mt-1.5 whitespace-pre-line text-xs leading-relaxed text-muted-foreground/80">{patch.note}</div>}
      </div>
      <Switch className="mt-1" checked={patch.enabled} onCheckedChange={(v) => onToggle(v)} />
    </div>
  );
}
