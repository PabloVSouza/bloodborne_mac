import { useTranslation } from "react-i18next";
import { Moon } from "@/components/brand";
import { PlayButton } from "@/components/sidebar";

/** The night sky banner with the moon and a large Play button. */
export function HomeHero() {
  const { t } = useTranslation();
  return (
    <div className="relative mb-6 overflow-hidden rounded-3xl border border-border bg-gradient-to-br from-blood-700/40 via-night-800 to-night-950 px-10 py-12 shadow-2xl shadow-black/50">
      <div className="pointer-events-none absolute -right-10 -top-10 opacity-90">
        <Moon className="size-72" />
      </div>
      <div className="pointer-events-none absolute inset-0 bg-[radial-gradient(ellipse_at_bottom_left,rgba(161,29,34,0.35),transparent_60%)]" />
      <div className="relative max-w-md">
        <h1 className="font-display text-5xl font-semibold tracking-wide text-parchment drop-shadow-lg">{t("home.title")}</h1>
        <p className="mt-3 text-sm text-parchment/70">{t("home.subtitle")}</p>
        <PlayButton className="mt-8 max-w-56 py-3.5 text-base" />
      </div>
    </div>
  );
}
