import { useTranslation } from "react-i18next";
import { RefreshCw } from "lucide-react";
import { Button, Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui";
import { SettingRow } from "@/components/settings";
import { useGamepads, useSetting, useSettingsData } from "@/hooks";

/** Which controller the game uses (BB_GAMEPAD): automatic, a connected one, or the saved one
 *  while it is not connected. */
export function GamepadPicker() {
  const { t } = useTranslation();
  const { settings } = useSettingsData();
  const setSetting = useSetting();
  const gamepads = useGamepads();
  const connected = gamepads.data ?? [];
  const items = [
    { value: "", label: t("controls.auto") },
    ...connected.map((g) => ({ value: g.guid, label: g.name })),
  ];
  if (settings.gamepad && !connected.some((g) => g.guid === settings.gamepad)) {
    items.push({ value: settings.gamepad, label: t("controls.notConnected", { name: settings.gamepad_name || settings.gamepad }) });
  }
  const choose = (guid: string) => {
    setSetting("gamepad", guid);
    setSetting("gamepad_name", connected.find((g) => g.guid === guid)?.name ?? (guid ? settings.gamepad_name : ""));
  };
  return (
    <SettingRow
      title={t("controls.controller")}
      description={connected.length === 0 && !gamepads.isPending ? t("controls.noControllers") : t("controls.controllerHint")}
    >
      <Select value={settings.gamepad ?? ""} items={items} onValueChange={(v) => v !== null && choose(v)}>
        <SelectTrigger className="min-w-64 bg-secondary">
          <SelectValue />
        </SelectTrigger>
        <SelectContent>
          {items.map((item) => (
            <SelectItem key={item.value || "auto"} value={item.value}>
              {item.label}
            </SelectItem>
          ))}
        </SelectContent>
      </Select>
      <Button variant="ghost" size="icon-sm" onClick={() => gamepads.refetch()} title={t("common.refresh")}>
        <RefreshCw className={gamepads.isFetching ? "animate-spin" : undefined} />
      </Button>
    </SettingRow>
  );
}
