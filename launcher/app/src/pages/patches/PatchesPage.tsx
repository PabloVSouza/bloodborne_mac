import { useTranslation } from "react-i18next";
import { FileCode2, RefreshCw } from "lucide-react";
import { Button } from "@/components/ui";
import { EmptyState, FolderRow, PageHeader, SettingsSection } from "@/components/settings";
import { PatchRow } from "@/pages/patches/PatchRow";
import { usePatches, useSetting, useSettingsData } from "@/hooks";

export function PatchesPage() {
  const { t } = useTranslation();
  const { settings } = useSettingsData();
  const setSetting = useSetting();
  const { data, refetch, isFetching, save } = usePatches();
  const patches = data?.patches ?? [];
  return (
    <>
      <PageHeader
        title={t("patches.title")}
        subtitle={t("patches.subtitle")}
        actions={
          <Button variant="secondary" size="sm" onClick={() => refetch()}>
            <RefreshCw className={isFetching ? "animate-spin" : undefined} /> {t("common.refresh")}
          </Button>
        }
      />
      <SettingsSection>
        <FolderRow
          title={t("patches.folder")}
          description={t("patches.folderHint")}
          path={data?.dir ?? ""}
          custom={!!settings.patches_dir}
          onChoose={(path) => setSetting("patches_dir", path)}
          onReset={() => setSetting("patches_dir", "")}
        />
      </SettingsSection>
      <SettingsSection title={t("patches.list")} description={t("common.restartNote")}>
        {patches.length === 0 ? (
          <EmptyState icon={<FileCode2 className="size-10" />} title={t("patches.empty")}>
            {t("patches.emptyHint")}
          </EmptyState>
        ) : (
          patches.map((patch) => (
            <PatchRow
              key={patch.key}
              patch={patch}
              onToggle={(enabled) => save(patches.map((p) => (p.key === patch.key ? { ...p, enabled } : p)))}
            />
          ))
        )}
      </SettingsSection>
    </>
  );
}
