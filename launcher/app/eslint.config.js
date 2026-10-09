// Launcher lint rules (npm run lint). Conventions: launcher/app/README.md.
import js from "@eslint/js";
import tseslint from "typescript-eslint";
import react from "eslint-plugin-react";
import reactHooks from "eslint-plugin-react-hooks";
import globals from "globals";

export default tseslint.config(
  { ignores: ["dist", "src-tauri", "node_modules"] },
  js.configs.recommended,
  ...tseslint.configs.recommended,
  {
    files: ["src/**/*.{ts,tsx}"],
    languageOptions: { globals: globals.browser },
    settings: { react: { version: "detect" } },
    plugins: { react, "react-hooks": reactHooks },
    rules: {
      ...reactHooks.configs.recommended.rules,
      // Parameters a signature needs but the body does not: named with a leading underscore.
      "@typescript-eslint/no-unused-vars": ["error", { argsIgnorePattern: "^_", varsIgnorePattern: "^_" }],
      // One component per file.
      "react/no-multi-comp": ["error", { ignoreStateless: false }],
      // Imports go through the "@/..." alias (src/), never relative paths.
      "no-restricted-imports": [
        "error",
        { patterns: [{ group: ["./*", "../*"], message: "Import through the @/ alias (src/)." }] },
      ],
    },
  },
  // shadcn/ui's generated components keep their upstream shape (several parts per file).
  { files: ["src/components/ui/**"], rules: { "react/no-multi-comp": "off", "react-hooks/purity": "off" } },
);
