import "@hitomi/env/web";
import { resolveAppListenHost, resolveAppPort } from "@hitomi/env/app";

export default defineNuxtConfig({
  compatibilityDate: "latest",
  ssr: false,
  modules: ["@nuxt/ui"],
  devtools: { enabled: true },
  ui: {
    fonts: false,
  },
  experimental: {
    payloadExtraction: "client",
  },
  css: ["~/assets/css/main.css"],
  devServer: {
    host: resolveAppListenHost(),
    port: resolveAppPort(),
  },
  runtimeConfig: {
    public: {
      serverUrl: process.env.NUXT_PUBLIC_SERVER_URL || "",
    },
  },
});
