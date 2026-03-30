<script setup lang="ts">
import SignInForm from "~/components/SignInForm.vue";
import SignUpForm from "~/components/SignUpForm.vue";
import { onMounted } from "vue";

const { $authClient } = useNuxtApp();

const session = $authClient.useSession();
const showSignIn = ref(true);
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
  <div class="workspace-auth-shell grid min-h-[100svh] place-items-center px-4 py-10 sm:px-6">
    <div class="w-full max-w-md">
      <div
        v-if="isSessionPending"
        class="workspace-auth-card flex min-h-80 flex-col items-center justify-center gap-4 p-8"
      >
        <UIcon name="i-lucide-loader-2" class="animate-spin text-4xl text-primary" />
        <span class="text-muted">加载中...</span>
      </div>

      <div v-else-if="!isSignedIn">
        <SignInForm v-if="showSignIn" @switch-to-sign-up="showSignIn = false" />
        <SignUpForm v-else @switch-to-sign-in="showSignIn = true" />
      </div>
    </div>
  </div>
</template>
