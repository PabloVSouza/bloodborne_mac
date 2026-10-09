import { useState } from "react";
import { useTranslation } from "react-i18next";
import { Copy, FolderOpen, ScrollText, Trash2 } from "lucide-react";
import { toast } from "sonner";
import { Button, Label, Switch } from "@/components/ui";
import { EmptyState, PageHeader } from "@/components/settings";
import { LogView } from "@/pages/log/LogView";
import { useConfig } from "@/hooks";
import { api } from "@/lib";
import { useGameStore } from "@/stores";

export function LogPage() {
  const { t } = useTranslation();
  const { data: config } = useConfig();
  const { log, clearLog } = useGameStore();
  const [follow, setFollow] = useState(true);
  const copy = async () => {
    await navigator.clipboard.writeText(log.join("\n"));
    toast.success(t("log.copied"));
  };
  return (
    <>
      <PageHeader
        title={t("log.title")}
        subtitle={t("log.subtitle")}
        actions={
          <>
            <div className="mr-2 flex items-center gap-2">
              <Switch id="follow" checked={follow} onCheckedChange={(v) => setFollow(v)} />
              <Label htmlFor="follow" className="text-xs text-muted-foreground">
                {t("log.follow")}
              </Label>
            </div>
            <Button variant="secondary" size="sm" onClick={copy} disabled={log.length === 0}>
              <Copy /> {t("log.copy")}
            </Button>
            <Button variant="secondary" size="sm" onClick={clearLog} disabled={log.length === 0}>
              <Trash2 /> {t("log.clear")}
            </Button>
            <Button variant="secondary" size="sm" onClick={() => config && api.openPath(config.paths.logs)}>
              <FolderOpen /> {t("log.openFolder")}
            </Button>
          </>
        }
      />
      {log.length === 0 ? (
        <div className="rounded-2xl border border-border bg-card/40">
          <EmptyState icon={<ScrollText className="size-10" />} title={t("log.empty")}>
            {t("log.emptyHint")}
          </EmptyState>
        </div>
      ) : (
        <LogView lines={log} follow={follow} />
      )}
    </>
  );
}
