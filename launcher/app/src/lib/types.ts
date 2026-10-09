// Shapes the Tauri backend (src-tauri/src/main.rs) sends and receives.

/** launcher.json: start-up options passed to run.sh as environment variables. */
export type Settings = {
  game_dir: string;
  user_dir: string;
  mods_dir: string;
  mods_enabled: boolean;
  patches_dir: string;
  language: string;
  fullscreen: boolean;
  hdr: boolean;
  present_mode: string;
  gamepad: string;
  gamepad_name: string;
  fps_mode: string;
  fps_limit: number;
  draw_pipe: string;
  readbacks: string;
  preupload: string;
  frame_stats: boolean;
  save_log: boolean;
  crash_diag: boolean;
  gpu_profile: boolean;
  extra_env: string;
  /** Launcher language ("" = the system's). */
  ui_language: string;
};

/** The game folder's edition, from its sce_sys/param.sfo (scripts/game_check.py describe). */
export type GameEdition = {
  title_id: string;
  title: string;
  version: string;
  region: "america" | "europe" | "japan" | "asia" | "unknown";
  /** A Game of the Year edition: The Old Hunters is part of the game. */
  old_hunters: boolean;
};

/** The game check: why the folder cannot start (null when it can) and its edition. */
export type GameCheck = { problem: string | null; game: GameEdition | null };

/** bbport.ini: the port's settings, shared with the in-game menu. */
export type Ini = Record<string, string>;

export type Config = {
  settings: Settings;
  ini: Ini;
  paths: { data: string; mods: string; patches: string; logs: string; saves: string };
  bundled: boolean;
  version: string;
  running: boolean;
};

export type Mod = { name: string; enabled: boolean };

export type Patch = {
  key: string;
  name: string;
  file: string;
  author: string;
  note: string;
  default: boolean;
  enabled: boolean;
};

export type Gamepad = { guid: string; name: string };

export type GameState = { running: boolean; code?: number | null };

/** An i18n key (src/i18n/locales/en.json). */
export type TKey = import("i18next").ParseKeys;

/** A choice: label and hint are i18n keys, or literal text when `literal` (e.g. language names). */
export type Option<T extends string | number> =
  | { value: T; label: TKey; hint?: TKey; literal?: false }
  | { value: T; label: string; hint?: undefined; literal: true };
