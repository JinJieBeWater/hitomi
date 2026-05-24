<script setup lang="ts">
import { onMounted } from "vue";

const { $authClient } = useNuxtApp();

const session = $authClient.useSession();
const mounted = ref(false);
const isSessionPending = computed(() => !mounted.value || session.value.isPending);
const isSignedIn = computed(() => Boolean(session.value.data));

watchEffect(() => {
  if (isSessionPending.value) {
    return;
  }

  if (isSignedIn.value) {
    navigateTo("/dashboard", { replace: true });
  }
});

onMounted(() => {
  mounted.value = true;
});
</script>

<template>
  <div class="grid min-h-[100svh] place-items-center px-4 py-10 sm:px-6">
    <UPageCard
      title="考勤管理后台"
      icon="i-lucide-shield-check"
      class="workspace-auth-card w-full max-w-sm"
    >
      <div v-if="isSessionPending" class="flex min-h-32 items-center justify-center">
        <UIcon name="i-lucide-loader-2" class="text-2xl text-primary animate-spin" />
      </div>

      <div v-else class="space-y-3">
        <UButton to="/login" block size="lg" icon="i-lucide-log-in">前往登录</UButton>
        <UButton to="/dashboard" block variant="outline" color="neutral">
          尝试进入后台
        </UButton>
      </div>
    </UPageCard>
  </div>
</template>
