export default defineNuxtRouteMiddleware(async () => {
  if (import.meta.server) return;

  const { $authClient } = useNuxtApp();
  const session = await $authClient.getSession();

  if (!session.data) {
    return navigateTo("/login");
  }
});
