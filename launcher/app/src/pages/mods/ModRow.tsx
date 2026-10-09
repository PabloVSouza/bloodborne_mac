import { useTranslation } from "react-i18next";
import { ArrowDown, ArrowUp, Package } from "lucide-react";
import { Button, Switch } from "@/components/ui";
import type { Mod } from "@/lib";

type Props = {
  mod: Mod;
  position: number;
  first: boolean;
  last: boolean;
  onToggle: (enabled: boolean) => void;
  onMove: (direction: -1 | 1) => void;
};

/** A mod in the load order: on/off, earlier/later. */
export function ModRow({ mod, position, first, last, onToggle, onMove }: Props) {
  const { t } = useTranslation();
  return (
    <div className="flex items-center gap-4 px-5 py-3">
      <span className="w-6 text-right text-xs tabular-nums text-muted-foreground">{position}</span>
      <Package className="size-4 text-muted-foreground" />
      <span className={mod.enabled ? "flex-1 text-sm" : "flex-1 text-sm text-muted-foreground line-through"}>{mod.name}</span>
      <Button variant="ghost" size="icon-sm" onClick={() => onMove(-1)} disabled={first} title={t("mods.moveUp")}>
        <ArrowUp />
      </Button>
      <Button variant="ghost" size="icon-sm" onClick={() => onMove(1)} disabled={last} title={t("mods.moveDown")}>
        <ArrowDown />
      </Button>
      <Switch checked={mod.enabled} onCheckedChange={(v) => onToggle(v)} />
    </div>
  );
}
