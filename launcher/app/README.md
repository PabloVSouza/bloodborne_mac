# Bloodborne launcher (macOS)

The launcher inside `Bloodborne.app`. It has a game folder check, graphics, controls (controller
choice and button mapping), game options, mods, patches, advanced options and the game's log. It
starts the game with the app's own bash and Python (`packaging/macos.sh` builds the app around
it).

- **Interface:** React 19, TypeScript, Tailwind CSS 4, [shadcn/ui](https://ui.shadcn.com)
  (Base UI), lucide icons, react-i18next.
- **Backend:** [Tauri 2](https://tauri.app) (`src-tauri/src/main.rs`). It reads and writes
  `launcher.json` and `bbport.ini` in `~/Library/Application Support/bloodborne_mac`, lists mods
  and patches through `scripts/launcher_api.py`, controllers through `bb-gpu-capabilities`, and
  runs `run.sh`.

```bash
npm ci
npm run dev          # the interface in a browser, with mock data (src/lib/mockApi.ts); #graphics etc. opens a tab
npm run tauri dev    # the app, against a checkout's build (out/bb-probe; bash build.sh first)
npm run check        # TypeScript and ESLint
npx tauri build      # Bloodborne.app (packaging/macos.sh adds the game to it)
```

## Conventions

These are enforced by ESLint (`eslint.config.js`) where possible.

- **One component per file** (`react/no-multi-comp`). shadcn's generated `src/components/ui` is
  exempt.
- **Imports go through the `@/` alias** (`src/`), never relative paths (`no-restricted-imports`).
- **Every folder has an `index.ts`** that exports its contents. Code outside a folder imports
  from the folder (`@/components/settings`, `@/hooks`, `@/lib/data`). Files inside a folder
  import their siblings by full path (`@/pages/controls/BindingRow`), so a folder's index is
  never imported from inside it.
- **A folder holds a feature and its parts.** For example, `components/sidebar` has the
  sidebar, its nav item and the Play button, and `pages/controls` has the page with its rows and
  pickers.
- **State:**
  - Backend data goes through **TanStack Query** (`src/hooks`): the config, the game check,
    mods, patches and controllers. Saves are mutations with optimistic updates.
  - Client state goes in **Zustand** stores (`src/stores`): the game process and its log, and
    the current tab.
  - Keep component-local state in `useState`.
- **All text comes from i18n** (`src/i18n/locales/*.json`). `en.json` is the reference: its keys
  are typed (`i18next.d.ts`), so a missing key is a type error. Option lists store keys
  (`src/lib/data`), with `literal: true` for names shown as they are, such as language names.
- **UI components come from shadcn** (`npx shadcn@latest add <name>`, then export them from
  `src/components/ui/index.ts`). The theme is the CSS variables in `src/index.css`.

## Layout

| Path | Contents |
|---|---|
| `src/components/ui` | shadcn/ui components |
| `src/components/{brand,settings,sidebar,window}` | Shared building blocks: setting rows, sidebar, window shell |
| `src/pages/<tab>` | One folder per tab, with the page and its parts |
| `src/hooks` | TanStack Query hooks and small effects |
| `src/stores` | Zustand stores |
| `src/lib` | Backend calls (`api.ts`, `mockApi.ts`), types, query client, `data/` (options, controls) |
| `src/i18n` | i18next setup and locales (English, Português do Brasil) |
| `src-tauri` | Rust backend, Tauri config, icons |
| `scripts/make_icon.swift` | Draws the icon the app's icons are made from |
