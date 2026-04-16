<script setup lang="ts">
import { onMounted } from "vue";

const props = withDefaults(
  defineProps<{
    collapsed?: boolean;
  }>(),
  {
    collapsed: false,
  },
);

const { $authClient } = useNuxtApp();
const session = $authClient.useSession();
const toast = useToast();
const mounted = ref(false);
const isSessionPending = computed(() => !mounted.value || session.value.isPending);
const isSignedIn = computed(() => Boolean(session.value.data?.user));

const skeletonClass = computed(() => {
  return props.collapsed
    ? "workspace-sidebar-control workspace-sidebar-control-square"
    : "workspace-sidebar-control workspace-sidebar-control-inline";
});

async function handleSignOut(): Promise<void> {
  try {
    await $authClient.signOut({
      fetchOptions: {
        onSuccess: async () => {
          toast.add({ title: "已退出登录" });
          await navigateTo("/", { replace: true });
        },
        onError: (error) => {
          toast.add({
            title: "退出登录失败",
            description: error?.error?.message || "未知错误",
          });
        },
      },
    });
  } catch (error: any) {
    toast.add({
      title: "退出登录时发生未预期错误",
      description: error.message || "请稍后重试。",
    });
  }
}

onMounted(() => {
  mounted.value = true;
});
</script>

<template>
  <div :class="props.collapsed ? 'flex justify-center' : 'w-full'">
    <USkeleton v-if="isSessionPending" :class="skeletonClass" />

    <UButton
      v-else-if="!isSignedIn"
      color="neutral"
      variant="outline"
      size="lg"
      icon="i-lucide-log-in"
      :square="props.collapsed"
      :block="!props.collapsed"
      :class="
        props.collapsed
          ? 'workspace-sidebar-control workspace-sidebar-control-square'
          : 'workspace-sidebar-control workspace-sidebar-control-inline workspace-sidebar-nav-label'
      "
      to="/login"
    >
      登录
    </UButton>

    <UButton
      v-else
      color="neutral"
      variant="outline"
      size="lg"
      icon="i-lucide-log-out"
      :square="props.collapsed"
      :block="!props.collapsed"
      :label="props.collapsed ? undefined : '退出登录'"
      :class="
        props.collapsed
          ? 'workspace-sidebar-control workspace-sidebar-control-square'
          : 'workspace-sidebar-control workspace-sidebar-control-inline workspace-sidebar-nav-label'
      "
      data-testid="sign-out-button"
      @click="handleSignOut()"
    />
  </div>
</template>
