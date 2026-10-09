import { LoaderCircle, Play, Square } from "lucide-react";
import { useTranslation } from "react-i18next";
import { Tooltip, TooltipContent, TooltipTrigger } from "@/components/ui";
import { useConfig, useGameCheck } from "@/hooks";
import { cn } from "@/lib";
import { useGameStore } from "@/stores";

/** Play (or Stop while the game runs); disabled with the reason while the game folder has a
 *  problem. */
export function PlayButton({ className }: { className?: string }) {
  const { t } = useTranslation();
  const { data: config } = useConfig();
  const check = useGameCheck(config?.settings.game_dir);
  const { running, play, stop } = useGameStore();
  const problem = check.data;

  if (running) {
    return (
      <button
        type="button"
        onClick={stop}
        className={cn(
          "flex w-full items-center justify-center gap-2 rounded-xl border border-blood-500/50 bg-night-800 py-3 font-display text-sm font-semibold tracking-[0.25em] text-blood-400 transition hover:bg-blood-700/30",
          className,
        )}
      >
        <Square className="size-4 fill-current" /> {t("play.stop").toUpperCase()}
      </button>
    );
  }
  const button = (
    <button
      type="button"
      onClick={play}
      disabled={check.isPending || !!problem}
      className={cn(
        "flex w-full items-center justify-center gap-2 rounded-xl bg-gradient-to-b from-blood-500 to-blood-700 py-3 font-display text-sm font-semibold tracking-[0.25em] text-parchment shadow-lg shadow-blood-700/40 transition hover:from-blood-400 hover:to-blood-600 disabled:cursor-not-allowed disabled:from-night-700 disabled:to-night-700 disabled:text-muted-foreground disabled:shadow-none",
        className,
      )}
    >
      {check.isPending ? <LoaderCircle className="size-4 animate-spin" /> : <Play className="size-4 fill-current" />} {t("play.play").toUpperCase()}
    </button>
  );
  if (!problem) return button;
  return (
    <Tooltip>
      <TooltipTrigger render={<span className="block w-full" />}>{button}</TooltipTrigger>
      <TooltipContent>{problem}</TooltipContent>
    </Tooltip>
  );
}
