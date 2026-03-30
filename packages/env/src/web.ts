import { createEnv } from "@t3-oss/env-nuxt";

/**
 * Nuxt env validation - validates at build time when imported in nuxt.config.ts
 * For runtime access in components/plugins, use useRuntimeConfig() instead:
 *   const config = useRuntimeConfig()
 *   config.public.serverUrl (set NUXT_PUBLIC_SERVER_URL only when client must call another origin)
 */
export const env = createEnv({
  client: {},
  emptyStringAsUndefined: true,
});
