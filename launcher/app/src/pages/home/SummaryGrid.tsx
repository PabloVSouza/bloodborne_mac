import { useTranslation } from "react-i18next";
import { Gamepad2, Gauge, MonitorCog, Package } from "lucide-react";
import { SummaryTile } from "@/pages/home/SummaryTile";
import { useMods, useSettingsData } from "@/hooks";
import { FPS_MODES, PRESETS, renderSize } from "@/lib/data";
import { useUiStore } from "@/stores";

/** Upscaler, frame rate, controller and mods at a glance; each opens its page. */
export function SummaryGrid() {
  const { t } = useTranslation();
  const { settings, ini } = useSettingsData();
  const mods = useMods();
  const setTab = useUiStore((s) => s.setTab);
  const upscaler = ini.upscaler ?? "fsr3";
  const preset = Number(ini.preset ?? 2);
  const fps = FPS_MODES.find((f) => f.value === settings.fps_mode);
  const activeMods = mods.data?.mods.filter((m) => m.enabled).length ?? 0;
  const presetLabel = PRESETS.find((p) => p.value === preset)?.label;
  return (
    <div className="mb-6">
      <h2 className="mb-3 font-display text-xs font-semibold uppercase tracking-[0.2em] text-blood-400">{t("home.summary")}</h2>
      <div className="grid grid-cols-2 gap-3 lg:grid-cols-4">
        <SummaryTile
          icon={<MonitorCog />}
          label={t("home.upscaler")}
          value={upscaler === "off" ? t("options.upscaler.off") : presetLabel ? t(presetLabel) : "FSR"}
          detail={t("home.renderSize", { size: renderSize(ini.output_res ?? "1920x1080", preset, upscaler) })}
          onClick={() => setTab("graphics")}
        />
        <SummaryTile
          icon={<Gauge />}
          label={t("home.frameRate")}
          value={fps && !fps.literal ? t(fps.label) : settings.fps_mode}
          onClick={() => setTab("game")}
        />
        <SummaryTile
          icon={<Gamepad2 />}
          label={t("home.controller")}
          value={settings.gamepad_name || t("common.auto")}
          onClick={() => setTab("controls")}
        />
        <SummaryTile
          icon={<Package />}
          label={t("home.mods")}
          value={settings.mods_enabled ? t("home.modsActive", { count: activeMods }) : t("home.modsOff")}
          onClick={() => setTab("mods")}
        />
      </div>
    </div>
  );
}
