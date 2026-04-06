<script setup lang="ts">
withDefaults(
  defineProps<{
    status: "pending" | "error" | "success";
    error?: string | null;
    empty: boolean;
    emptyTitle?: string;
    emptyDescription?: string;
    emptyIcon?: string;
    skeletonCount?: number;
    skeletonClass?: string;
  }>(),
  {
    emptyTitle: "暂无数据",
    skeletonCount: 3,
    skeletonClass: "h-12 w-full rounded-2xl",
  },
);
</script>

<template>
  <div v-if="status === 'pending'" class="space-y-3">
    <USkeleton v-for="i in skeletonCount" :key="i" :class="skeletonClass" />
  </div>

  <UAlert
    v-else-if="status === 'error'"
    color="error"
    icon="i-lucide-alert-circle"
    title="加载失败"
    :description="error || '请稍后重试'"
  />

  <EmptyState
    v-else-if="empty"
    :title="emptyTitle"
    :description="emptyDescription"
    :icon="emptyIcon"
  >
    <template v-if="$slots['empty-actions']" #actions>
      <slot name="empty-actions" />
    </template>
  </EmptyState>

  <slot v-else />
</template>
