import { useTranslation } from "react-i18next";
import { CircleAlert, CircleCheck, LoaderCircle } from "lucide-react";
import { FolderRow, SettingsSection } from "@/components/settings";
import { useConfig, useGameCheck, useSetting } from "@/hooks";

/** The game folder and whether it can start. */
export function GameFolderCard() {
  const { t } = useTranslation();
  const { data: config } = useConfig();
  const setSetting = useSetting();
  const dir = config?.settings.game_dir ?? "";
  const check = useGameCheck(config?.settings.game_dir);
  const status = check.isPending ? (
    <span className="flex items-center gap-2 text-muted-foreground">
      <LoaderCircle className="size-4 animate-spin" /> {t("home.checking")}
    </span>
  ) : check.data ? (
    <span className="flex items-center gap-2 text-blood-400">
      <CircleAlert className="size-4 shrink-0" /> {dir ? check.data : t("home.noFolder")}
    </span>
  ) : (
    <span className="flex items-center gap-2 text-emerald-400">
      <CircleCheck className="size-4" /> {t("home.ready")}
    </span>
  );
  return (
    <SettingsSection title={t("home.gameFolder")} description={t("home.gameFolderHint")}>
      <FolderRow title="CUSA03173" path={dir} onChoose={(path) => setSetting("game_dir", path)} />
      <div className="px-5 py-3 text-sm">{status}</div>
    </SettingsSection>
  );
}
