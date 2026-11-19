import react from "@vitejs/plugin-react";
import { defineConfig } from "vite";

// `npm run dev` serves the page with live reload and passes API calls to the running server.
export default defineConfig({
  plugins: [react()],
  server: { proxy: { "/api": "http://localhost:8080", "/setup": "http://localhost:8080" } },
});
