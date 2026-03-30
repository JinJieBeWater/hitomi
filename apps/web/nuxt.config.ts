import "@hitomi/env/web";
import { resolveAppPort } from "@hitomi/env/app";

export default defineNuxtConfig({
  compatibilityDate: "latest",
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
    port: resolveAppPort(),
  },
  runtimeConfig: {
    public: {
      serverUrl: process.env.NUXT_PUBLIC_SERVER_URL || "",
    },
  },
});
