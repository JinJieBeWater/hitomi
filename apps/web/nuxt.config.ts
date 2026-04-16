import "@hitomi/env/web";
import { resolveAppListenHost, resolveAppOrigin, resolveAppPort } from "@hitomi/env/app";

export default defineNuxtConfig({
  compatibilityDate: "latest",
  ssr: false,
  modules: ["@nuxt/ui"],
  colorMode: {
    preference: "light",
  },
  devtools: { enabled: true },
  ui: {
    fonts: false,
  },
  experimental: {
    payloadExtraction: "client",
  },
  css: ["~/assets/css/main.css", "@xterm/xterm/css/xterm.css"],
  devServer: {
    host: resolveAppListenHost(),
    port: resolveAppPort(),
  },
  runtimeConfig: {
    public: {
      serverUrl: resolveAppOrigin(process.env),
    },
  },
});
