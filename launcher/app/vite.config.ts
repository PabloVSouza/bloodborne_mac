import path from "node:path";
import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import tailwindcss from "@tailwindcss/vite";

// Tauri serves the build from ../dist (src-tauri/tauri.conf.json) and the dev server on 5173.
// "@/..." is src/ (tsconfig.json paths, components.json aliases).
export default defineConfig({
  plugins: [react(), tailwindcss()],
  resolve: { alias: { "@": path.resolve(import.meta.dirname, "src") } },
  clearScreen: false,
  server: { port: 5173, strictPort: true },
  build: { target: "safari16", outDir: "dist", emptyOutDir: true, chunkSizeWarningLimit: 2000 },
});
