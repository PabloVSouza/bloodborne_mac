import { AppWindow, LoadingScreen } from "@/components/window";
import { useConfig, useGameConnection, useLanguageSync } from "@/hooks";

export function App() {
  const { data } = useConfig();
  useLanguageSync();
  useGameConnection();
  return data ? <AppWindow /> : <LoadingScreen />;
}
