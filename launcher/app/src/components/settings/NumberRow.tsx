import type { ReactNode } from "react";
import { Input } from "@/components/ui";
import { SettingRow } from "@/components/settings/SettingRow";

type Props = {
  title: ReactNode;
  description?: ReactNode;
  value: number;
  min: number;
  max: number;
  onChange: (value: number) => void;
};

export function NumberRow({ title, description, value, min, max, onChange }: Props) {
  return (
    <SettingRow title={title} description={description}>
      <Input
        type="number"
        className="w-24 bg-secondary text-right tabular-nums"
        min={min}
        max={max}
        value={value}
        onChange={(e) => {
          const next = Number(e.target.value);
          if (Number.isFinite(next)) onChange(Math.min(max, Math.max(min, Math.round(next))));
        }}
      />
    </SettingRow>
  );
}
