import { useState } from "react";
import { useTranslation } from "react-i18next";
import { Crosshair, LoaderCircle, RotateCcw } from "lucide-react";
import { toast } from "sonner";
import { Badge, Button, Tooltip, TooltipContent, TooltipTrigger } from "@/components/ui";
import { useIni, useSettingsData } from "@/hooks";
import { api } from "@/lib";
import { padLabel } from "@/lib/data";
import type { Control } from "@/lib/data";

type Props = { control: Control; kind: "pad" | "key" };

/** One input's binding: the current key or button, assign by pressing it, back to the default. */
export function BindingRow({ control, kind }: Props) {
  const { t } = useTranslation();
  const { ini } = useSettingsData();
  const setIni = useIni();
  const [capturing, setCapturing] = useState(false);
  const iniKey = `${kind}.${control.id}`;
  const fallback = kind === "pad" ? control.pad : control.key;
  const custom = iniKey in ini;
  const binding = custom ? ini[iniKey] : (fallback ?? "");
  const shown = binding ? (kind === "pad" ? padLabel(binding, t("controls.or")) : binding) : t("common.notAssigned");

  const assign = async () => {
    setCapturing(true);
    try {
      const pressed = await api.readInput(kind);
      if (pressed) setIni(iniKey, pressed);
    } catch (error) {
      toast.error(t("common.saveFailed"), { description: String(error) });
    } finally {
      setCapturing(false);
    }
  };

  if (kind === "pad" && control.pad === null) {
    return (
      <div className="flex items-center justify-between px-5 py-2.5 text-sm opacity-50">
        <span>{t(`controls.inputs.${control.id}` as "controls.inputs.cross")}</span>
        <span className="text-xs text-muted-foreground">{t("controls.keyboardOnly")}</span>
      </div>
    );
  }
  return (
    <div className="flex items-center justify-between gap-4 px-5 py-2.5">
      <div className="flex items-center gap-3 text-sm">
        {control.glyph && (
          <span className="flex size-7 items-center justify-center rounded-full bg-secondary text-sm text-blood-400">{control.glyph}</span>
        )}
        <span>{t(`controls.inputs.${control.id}` as "controls.inputs.cross")}</span>
      </div>
      <div className="flex items-center gap-2">
        {capturing ? (
          <span className="flex items-center gap-2 text-xs text-blood-400">
            <LoaderCircle className="size-3.5 animate-spin" /> {t("controls.press")}
          </span>
        ) : (
          <Badge variant={custom ? "default" : "secondary"} className="font-mono text-[11px]">
            {shown}
          </Badge>
        )}
        <Button variant="secondary" size="sm" onClick={assign} disabled={capturing}>
          <Crosshair /> {t("controls.assign")}
        </Button>
        <Tooltip>
          <TooltipTrigger render={<Button variant="ghost" size="icon-sm" onClick={() => setIni(iniKey, null)} disabled={!custom} />}>
            <RotateCcw />
          </TooltipTrigger>
          <TooltipContent>{t("common.reset")}</TooltipContent>
        </Tooltip>
      </div>
    </div>
  );
}
