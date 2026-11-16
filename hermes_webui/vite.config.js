import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

export default defineConfig({
  plugins: [react()],
  server: {
    host: "0.0.0.0",
    port: 45500,
    strictPort: true,
    allowedHosts: true,
    cors: true,
    headers: {
      "Access-Control-Allow-Origin": "*",
      "Access-Control-Allow-Methods": "GET, POST, PUT, PATCH, DELETE, OPTIONS",
      "Access-Control-Allow-Headers": "*",
    },
    proxy: {
      "/api": {
        target: "http://127.0.0.1:45501",
        changeOrigin: true,
      },
      "/ws": {
        target: "ws://127.0.0.1:45501",
        ws: true,
        changeOrigin: true,
      },
    },
  },
  preview: {
    host: "0.0.0.0",
    port: 45500,
    strictPort: true,
    allowedHosts: true,
    cors: true,
  },
});
