async function waitForSessionReady(session: ReturnType<ReturnType<typeof useNuxtApp>["$authClient"]["useSession"]>) {
  if (!session.value.isPending) {
    return;
  }

  await new Promise<void>((resolve) => {
    let stop: null | (() => void) = null;

    stop = watch(
      () => session.value.isPending,
      (isPending) => {
        if (!isPending) {
          stop?.();
          resolve();
        }
      },
      {
        immediate: true,
      },
    );
  });
}

export default defineNuxtRouteMiddleware(async (to, from) => {
  if (import.meta.server) return;

  const { $authClient } = useNuxtApp();
  const session = $authClient.useSession();

  await waitForSessionReady(session);

  if (!session.value.data) {
    return navigateTo("/login");
  }
});
