import type { ReactNode } from "react";
import { Switch } from "@/components/ui";
import { SettingRow } from "@/components/settings/SettingRow";

type Props = {
  title: ReactNode;
  description?: ReactNode;
  checked: boolean;
  onChange: (checked: boolean) => void;
  disabled?: boolean;
};

export function SwitchRow({ title, description, checked, onChange, disabled }: Props) {
  return (
    <SettingRow title={title} description={description} disabled={disabled}>
      <Switch checked={checked} onCheckedChange={(value) => onChange(value)} />
    </SettingRow>
  );
}
