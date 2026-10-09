import type { ReactNode } from "react";
import { Slider } from "@/components/ui";
import { SettingRow } from "@/components/settings/SettingRow";

type Props = {
  title: ReactNode;
  description?: ReactNode;
  value: number;
  min: number;
  max: number;
  step: number;
  onChange: (value: number) => void;
  format?: (value: number) => string;
  disabled?: boolean;
};

export function SliderRow({ title, description, value, min, max, step, onChange, format, disabled }: Props) {
  return (
    <SettingRow title={title} description={description} disabled={disabled}>
      <div className="w-48">
        <Slider
          min={min}
          max={max}
          step={step}
          value={[value]}
          onValueChange={(next) => onChange(Array.isArray(next) ? next[0] : (next as number))}
        />
      </div>
      <span className="w-10 text-right text-xs tabular-nums text-muted-foreground">{format ? format(value) : value}</span>
    </SettingRow>
  );
}
