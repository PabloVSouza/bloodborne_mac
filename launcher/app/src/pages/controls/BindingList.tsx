import { useTranslation } from "react-i18next";
import { BindingRow } from "@/pages/controls/BindingRow";
import { CONTROLS, CONTROL_GROUPS } from "@/lib/data";

/** All inputs of one kind (controller or keyboard), by group. */
export function BindingList({ kind }: { kind: "pad" | "key" }) {
  const { t } = useTranslation();
  return (
    <div>
      {CONTROL_GROUPS.map((group) => (
        <div key={group}>
          <div className="bg-night-900/60 px-5 py-2 text-[11px] uppercase tracking-[0.18em] text-muted-foreground">
            {t(`controls.groups.${group}`)}
          </div>
          <div className="divide-y divide-border">
            {CONTROLS.filter((c) => c.group === group).map((control) => (
              <BindingRow key={control.id} control={control} kind={kind} />
            ))}
          </div>
        </div>
      ))}
    </div>
  );
}
