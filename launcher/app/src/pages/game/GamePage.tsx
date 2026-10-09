import { useTranslation } from "react-i18next";
import { FolderRow, NumberRow, PageHeader, SelectRow, SettingsSection, SwitchRow } from "@/components/settings";
import { useConfig, useIni, useSetting, useSettingsData } from "@/hooks";
import { EFFECTS, FPS_MODES, GAME_LANGUAGES } from "@/lib/data";

export function GamePage() {
  const { t } = useTranslation();
  const { data: config } = useConfig();
  const { settings, ini } = useSettingsData();
  const setIni = useIni();
  const setSetting = useSetting();
  const flag = (key: string, fallback: boolean) => (key in ini ? ini[key] === "1" : fallback);
  return (
    <>
      <PageHeader title={t("game.title")} subtitle={t("game.subtitle")} />
      <SettingsSection title={t("game.language")}>
        <SelectRow
          title={t("game.systemLanguage")}
          description={t("game.systemLanguageHint")}
          value={settings.language}
          options={GAME_LANGUAGES}
          onChange={(v) => setSetting("language", v)}
        />
      </SettingsSection>
      <SettingsSection title={t("game.effects")} description={`${t("game.effectsHint")} ${t("common.restartNote")}`}>
        {EFFECTS.map((effect) => (
          <SwitchRow
            key={effect.key}
            title={t(effect.label)}
            description={effect.hint && t(effect.hint)}
            checked={flag(effect.key, effect.defaultOn)}
            onChange={(v) => setIni(effect.key, v ? "1" : "0")}
          />
        ))}
      </SettingsSection>
      <SettingsSection title={t("game.frameRate")}>
        <SelectRow title={t("game.fpsMode")} value={settings.fps_mode} options={FPS_MODES} onChange={(v) => setSetting("fps_mode", v)} />
        <NumberRow
          title={t("game.fpsLimit")}
          description={t("game.fpsLimitHint")}
          value={settings.fps_limit ?? 0}
          min={0}
          max={480}
          onChange={(v) => setSetting("fps_limit", v)}
        />
      </SettingsSection>
      <SettingsSection title={t("game.extras")} description={t("common.restartNote")}>
        <SwitchRow title={t("game.skipIntro")} checked={flag("skip_intro", false)} onChange={(v) => setIni("skip_intro", v ? "1" : "0")} />
        <SwitchRow
          title={t("game.debugCamera")}
          description={t("game.debugCameraHint")}
          checked={flag("debug_camera", false)}
          onChange={(v) => setIni("debug_camera", v ? "1" : "0")}
        />
        <SwitchRow
          title={t("game.debugMenu")}
          description={t("game.debugMenuHint")}
          checked={flag("debug_menu", false)}
          onChange={(v) => setIni("debug_menu", v ? "1" : "0")}
        />
      </SettingsSection>
      <SettingsSection title={t("game.saves")}>
        <FolderRow
          title={t("game.savesFolder")}
          path={config?.paths.saves ?? ""}
          custom={!!settings.user_dir}
          onChoose={(path) => setSetting("user_dir", path)}
          onReset={() => setSetting("user_dir", "")}
        />
      </SettingsSection>
    </>
  );
}
