import { useTranslation } from "react-i18next";
import { FolderRow, PageHeader, SelectRow, SettingsSection, SwitchRow, TextRow } from "@/components/settings";
import { useConfig, useIni, useSetting, useSettingsData } from "@/hooks";
import { LAUNCHER_LANGUAGES } from "@/i18n";
import { DRAW_PIPE, LIVE_RESOLUTION, PREUPLOAD, READBACKS } from "@/lib/data";
import type { Option } from "@/lib";

export function AdvancedPage() {
  const { t } = useTranslation();
  const { data: config } = useConfig();
  const { settings, ini } = useSettingsData();
  const setSetting = useSetting();
  const setIni = useIni();
  const languages: Option<string>[] = [
    { value: "", label: t("advanced.systemDefault"), literal: true },
    ...LAUNCHER_LANGUAGES.map((l) => ({ value: l.code, label: l.name, literal: true as const })),
  ];
  return (
    <>
      <PageHeader title={t("advanced.title")} subtitle={t("advanced.subtitle")} />
      <SettingsSection title={t("advanced.launcher")}>
        <SelectRow
          title={t("advanced.launcherLanguage")}
          description={t("advanced.launcherLanguageHint")}
          value={settings.ui_language ?? ""}
          options={languages}
          onChange={(v) => setSetting("ui_language", v)}
        />
      </SettingsSection>
      <SettingsSection title={t("advanced.performance")}>
        <SelectRow title={t("advanced.drawPipe")} value={settings.draw_pipe} options={DRAW_PIPE} onChange={(v) => setSetting("draw_pipe", v)} />
        <SelectRow title={t("advanced.readbacks")} value={settings.readbacks} options={READBACKS} onChange={(v) => setSetting("readbacks", v)} />
        <SelectRow title={t("advanced.preupload")} value={settings.preupload} options={PREUPLOAD} onChange={(v) => setSetting("preupload", v)} />
        <SelectRow
          title={t("advanced.liveResolution")}
          description={t("advanced.liveResolutionHint")}
          value={ini.live_resolution ?? "0"}
          options={LIVE_RESOLUTION}
          onChange={(v) => setIni("live_resolution", v)}
        />
      </SettingsSection>
      <SettingsSection title={t("advanced.diagnostics")}>
        <SwitchRow title={t("advanced.frameStats")} checked={settings.frame_stats} onChange={(v) => setSetting("frame_stats", v)} />
        <SwitchRow
          title={t("advanced.saveLog")}
          description={t("advanced.saveLogHint")}
          checked={settings.save_log}
          onChange={(v) => setSetting("save_log", v)}
        />
        <SwitchRow
          title={t("advanced.crashDiag")}
          description={t("advanced.crashDiagHint")}
          checked={settings.crash_diag}
          onChange={(v) => setSetting("crash_diag", v)}
        />
        <SwitchRow title={t("advanced.gpuProfile")} checked={settings.gpu_profile} onChange={(v) => setSetting("gpu_profile", v)} />
        <TextRow
          title={t("advanced.extraEnv")}
          description={t("advanced.extraEnvHint")}
          value={settings.extra_env ?? ""}
          placeholder="BB_EXAMPLE=1"
          onChange={(v) => setSetting("extra_env", v)}
        />
      </SettingsSection>
      <SettingsSection title={t("advanced.data")}>
        <FolderRow
          title={t("advanced.dataFolder")}
          description={t("advanced.dataFolderHint")}
          path={config?.paths.data ?? ""}
        />
      </SettingsSection>
    </>
  );
}
