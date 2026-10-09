import type { ReactNode } from "react";
import { useTranslation } from "react-i18next";
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui";
import { SettingRow } from "@/components/settings/SettingRow";
import type { Option } from "@/lib";

type Props<T extends string | number> = {
  title: ReactNode;
  description?: ReactNode;
  value: T;
  options: Option<T>[];
  onChange: (value: T) => void;
  disabled?: boolean;
};

/** A setting with a list of choices; the chosen one's hint is shown under the title. */
export function SelectRow<T extends string | number>({ title, description, value, options, onChange, disabled }: Props<T>) {
  const { t } = useTranslation();
  const label = (o: Option<T>) => (o.literal ? o.label : t(o.label));
  const chosen = options.find((o) => o.value === value);
  const hint = chosen && !chosen.literal && chosen.hint ? t(chosen.hint) : undefined;
  return (
    <SettingRow title={title} description={hint ?? description} disabled={disabled}>
      <Select
        value={value}
        items={options.map((o) => ({ value: o.value, label: label(o) }))}
        onValueChange={(next) => next !== null && onChange(next as T)}
      >
        <SelectTrigger className="min-w-52 bg-secondary">
          <SelectValue />
        </SelectTrigger>
        <SelectContent>
          {options.map((o) => (
            <SelectItem key={String(o.value)} value={o.value}>
              {label(o)}
            </SelectItem>
          ))}
        </SelectContent>
      </Select>
    </SettingRow>
  );
}
