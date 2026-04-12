import { createAuthClient } from "better-auth/vue";

export default defineNuxtPlugin(() => {
  const authClient = createAuthClient({});

  return {
    provide: {
      authClient: authClient,
    },
  };
});
