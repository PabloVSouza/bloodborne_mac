import type { ReactNode } from "react";
import { useTranslation } from "react-i18next";
import { open } from "@tauri-apps/plugin-dialog";
import { FolderOpen, FolderSearch, RotateCcw } from "lucide-react";
import { Button, Tooltip, TooltipContent, TooltipTrigger } from "@/components/ui";
import { SettingRow } from "@/components/settings/SettingRow";
import { api } from "@/lib";

type Props = {
  title: ReactNode;
  description?: ReactNode;
  /** The folder in use (the default when none was chosen). */
  path: string;
  /** Whether a folder was chosen (else the default is used). */
  custom?: boolean;
  /** Without it the folder cannot be changed (shown and revealed only). */
  onChoose?: (path: string) => void;
  onReset?: () => void;
};

/** A folder: its path, a chooser, Reveal in Finder, and back to the default. */
export function FolderRow({ title, description, path, custom, onChoose, onReset }: Props) {
  const { t } = useTranslation();
  const choose = async () => {
    const picked = await open({ directory: true, defaultPath: path || undefined, title: String(title) });
    if (typeof picked === "string") onChoose?.(picked);
  };
  return (
    <SettingRow
      title={title}
      description={
        <>
          {description && <span className="block">{description}</span>}
          <span className="mt-1 block max-w-md truncate font-mono text-[11px] text-muted-foreground/80" title={path}>
            {path || "—"}
          </span>
        </>
      }
    >
      {onChoose && (
        <Button variant="secondary" size="sm" onClick={choose}>
          <FolderSearch /> {t("common.choose")}
        </Button>
      )}
      <Tooltip>
        <TooltipTrigger render={<Button variant="ghost" size="icon-sm" onClick={() => api.openPath(path)} disabled={!path} />}>
          <FolderOpen />
        </TooltipTrigger>
        <TooltipContent>{t("common.showInFinder")}</TooltipContent>
      </Tooltip>
      {onReset && custom && (
        <Tooltip>
          <TooltipTrigger render={<Button variant="ghost" size="icon-sm" onClick={onReset} />}>
            <RotateCcw />
          </TooltipTrigger>
          <TooltipContent>{t("common.useDefault")}</TooltipContent>
        </Tooltip>
      )}
    </SettingRow>
  );
}
