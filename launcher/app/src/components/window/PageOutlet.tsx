import { AdvancedPage, ControlsPage, GamePage, GraphicsPage, HomePage, LogPage, ModsPage, PatchesPage } from "@/pages";
import { useUiStore } from "@/stores";

/** The page of the chosen tab. */
export function PageOutlet() {
  const tab = useUiStore((s) => s.tab);
  switch (tab) {
    case "home":
      return <HomePage />;
    case "graphics":
      return <GraphicsPage />;
    case "controls":
      return <ControlsPage />;
    case "game":
      return <GamePage />;
    case "mods":
      return <ModsPage />;
    case "patches":
      return <PatchesPage />;
    case "advanced":
      return <AdvancedPage />;
    case "log":
      return <LogPage />;
  }
}
