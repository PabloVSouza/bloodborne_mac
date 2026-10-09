import { Moon } from "@/components/brand";

export function LoadingScreen() {
  return (
    <div className="flex h-full items-center justify-center">
      <Moon className="size-20 animate-pulse" />
    </div>
  );
}
