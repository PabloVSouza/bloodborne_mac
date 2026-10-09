import { useTranslation } from "react-i18next";
import { FolderOpen, Info } from "lucide-react";
import { Alert, AlertDescription, Button } from "@/components/ui";
import { GameFolderCard } from "@/pages/home/GameFolderCard";
import { HomeHero } from "@/pages/home/HomeHero";
import { SummaryGrid } from "@/pages/home/SummaryGrid";
import { useConfig } from "@/hooks";
import { api } from "@/lib";
import { useGameStore } from "@/stores";

export function HomePage() {
  const { t } = useTranslation();
  const { data: config } = useConfig();
  const lastExit = useGameStore((s) => s.lastExit);
  return (
    <>
      <HomeHero />
      {lastExit !== undefined && (
        <Alert className="mb-5" variant={lastExit === 0 || lastExit === null ? "default" : "destructive"}>
          <Info />
          <AlertDescription>
            {lastExit === 0 || lastExit === null ? t("home.lastRunOk") : t("home.lastRunError", { code: lastExit })}
          </AlertDescription>
        </Alert>
      )}
      <GameFolderCard />
      <SummaryGrid />
      <div className="flex items-center justify-between gap-4 rounded-2xl border border-border bg-card/40 px-5 py-4 text-xs text-muted-foreground">
        <span>{t("home.firstStart")}</span>
        <Button variant="secondary" size="sm" onClick={() => config && api.openPath(config.paths.data)}>
          <FolderOpen /> {t("home.openData")}
        </Button>
      </div>
    </>
  );
}
