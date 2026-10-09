import { useState } from "react";
import type { ReactNode } from "react";
import { Input } from "@/components/ui";
import { SettingRow } from "@/components/settings/SettingRow";

type Props = { title: ReactNode; description?: ReactNode; value: string; onChange: (value: string) => void; placeholder?: string };

/** A text setting, saved when the field loses focus or Return is pressed. */
export function TextRow({ title, description, value, onChange, placeholder }: Props) {
  const [draft, setDraft] = useState(value);
  const commit = () => draft !== value && onChange(draft);
  return (
    <SettingRow title={title} description={description}>
      <Input
        className="w-80 bg-secondary font-mono text-xs"
        spellCheck={false}
        value={draft}
        placeholder={placeholder}
        onChange={(e) => setDraft(e.target.value)}
        onBlur={commit}
        onKeyDown={(e) => e.key === "Enter" && commit()}
      />
    </SettingRow>
  );
}
