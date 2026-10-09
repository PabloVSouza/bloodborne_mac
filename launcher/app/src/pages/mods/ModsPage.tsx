import { useTranslation } from "react-i18next";
import { PackageOpen, RefreshCw } from "lucide-react";
import { Button } from "@/components/ui";
import { EmptyState, FolderRow, PageHeader, SettingsSection, SwitchRow } from "@/components/settings";
import { ModRow } from "@/pages/mods/ModRow";
import { useMods, useSetting, useSettingsData } from "@/hooks";

export function ModsPage() {
  const { t } = useTranslation();
  const { settings } = useSettingsData();
  const setSetting = useSetting();
  const { data, refetch, isFetching, save } = useMods();
  const mods = data?.mods ?? [];
  const move = (index: number, direction: -1 | 1) => {
    const next = [...mods];
    const target = index + direction;
    [next[index], next[target]] = [next[target], next[index]];
    save(next);
  };
  return (
    <>
      <PageHeader
        title={t("mods.title")}
        subtitle={t("mods.subtitle")}
        actions={
          <Button variant="secondary" size="sm" onClick={() => refetch()}>
            <RefreshCw className={isFetching ? "animate-spin" : undefined} /> {t("common.refresh")}
          </Button>
        }
      />
      <SettingsSection>
        <SwitchRow title={t("mods.enabled")} checked={settings.mods_enabled} onChange={(v) => setSetting("mods_enabled", v)} />
        <FolderRow
          title={t("mods.folder")}
          description={t("mods.folderHint")}
          path={data?.dir ?? ""}
          custom={!!settings.mods_dir}
          onChoose={(path) => setSetting("mods_dir", path)}
          onReset={() => setSetting("mods_dir", "")}
        />
      </SettingsSection>
      <SettingsSection title={t("mods.list")} className={settings.mods_enabled ? undefined : "opacity-50"}>
        {mods.length === 0 ? (
          <EmptyState icon={<PackageOpen className="size-10" />} title={t("mods.empty")}>
            {t("mods.emptyHint")}
          </EmptyState>
        ) : (
          mods.map((mod, index) => (
            <ModRow
              key={mod.name}
              mod={mod}
              position={index + 1}
              first={index === 0}
              last={index === mods.length - 1}
              onToggle={(enabled) => save(mods.map((m) => (m.name === mod.name ? { ...m, enabled } : m)))}
              onMove={(direction) => move(index, direction)}
            />
          ))
        )}
      </SettingsSection>
    </>
  );
}
