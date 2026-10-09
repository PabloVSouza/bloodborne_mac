import { Sidebar } from "@/components/sidebar";
import { PageOutlet } from "@/components/window/PageOutlet";
import { TitleBar } from "@/components/window/TitleBar";

/** The launcher window: sidebar with the tabs and Play, the chosen page on the right. */
export function AppWindow() {
  return (
    <div className="flex h-full">
      <Sidebar />
      <main className="relative flex flex-1 flex-col overflow-y-auto bg-[radial-gradient(ellipse_at_top_right,rgba(122,20,24,0.18),transparent_55%)]">
        <TitleBar />
        <div className="mx-auto w-full max-w-4xl px-10 pb-12">
          <PageOutlet />
        </div>
      </main>
    </div>
  );
}
