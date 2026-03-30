import { createAuthClient } from "better-auth/vue";

export default defineNuxtPlugin(() => {
  const config = useRuntimeConfig();
  const baseURL = config.public.serverUrl || undefined;

  const authClient = createAuthClient(baseURL ? { baseURL } : {});

  return {
    provide: {
      authClient: authClient,
    },
  };
});
