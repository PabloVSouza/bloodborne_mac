import { useTranslation } from "react-i18next";
import { SettingRow } from "@/components/settings";
import { Badge } from "@/components/ui";
import type { GameEdition } from "@/lib";

/** The dump's edition: title ID, region and version, and whether The Old Hunters is included. */
export function GameEditionRow({ game }: { game: GameEdition }) {
  const { t } = useTranslation();
  const version = game.version.replace(/^0/, "");
  return (
    <SettingRow
      title={`${game.title || "Bloodborne"} · ${t(`home.region.${game.region}`)}`}
      description={t("home.edition", { id: game.title_id, version })}
    >
      <span className="text-xs text-muted-foreground">{t("home.oldHunters")}</span>
      {game.old_hunters ? (
        <Badge className="bg-emerald-500/15 text-emerald-400">{t("home.dlcIncluded")}</Badge>
      ) : (
        <Badge variant="outline" title={t("home.dlcMissingHint")}>
          {t("home.dlcMissing")}
        </Badge>
      )}
    </SettingRow>
  );
}
