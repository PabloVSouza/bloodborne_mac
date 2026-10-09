import { useTranslation } from "react-i18next";
import { Gamepad2, Keyboard, RotateCcw } from "lucide-react";
import { Button, Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui";
import { PageHeader, SettingsSection } from "@/components/settings";
import { BindingList } from "@/pages/controls/BindingList";
import { GamepadPicker } from "@/pages/controls/GamepadPicker";
import { useIni, useSettingsData } from "@/hooks";
import { CONTROLS } from "@/lib/data";

export function ControlsPage() {
  const { t } = useTranslation();
  const { ini } = useSettingsData();
  const setIni = useIni();
  const resetAll = () => {
    for (const control of CONTROLS) {
      for (const kind of ["pad", "key"]) {
        if (`${kind}.${control.id}` in ini) setIni(`${kind}.${control.id}`, null);
      }
    }
  };
  return (
    <>
      <PageHeader
        title={t("controls.title")}
        subtitle={t("controls.subtitle")}
        actions={
          <Button variant="secondary" size="sm" onClick={resetAll}>
            <RotateCcw /> {t("controls.resetAll")}
          </Button>
        }
      />
      <SettingsSection>
        <GamepadPicker />
      </SettingsSection>
      <Tabs defaultValue="pad">
        <TabsList className="mb-3">
          <TabsTrigger value="pad">
            <Gamepad2 /> {t("controls.gamepad")}
          </TabsTrigger>
          <TabsTrigger value="key">
            <Keyboard /> {t("controls.keyboard")}
          </TabsTrigger>
        </TabsList>
        <TabsContent value="pad">
          <SettingsSection>
            <BindingList kind="pad" />
          </SettingsSection>
        </TabsContent>
        <TabsContent value="key">
          <SettingsSection>
            <BindingList kind="key" />
          </SettingsSection>
        </TabsContent>
      </Tabs>
    </>
  );
}
