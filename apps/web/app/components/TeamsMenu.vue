<script setup lang="ts">
const props = defineProps<{
  collapsed?: boolean;
}>();
</script>

<template>
  <NuxtLink
    to="/dashboard"
    class="group flex items-center transition-colors"
    :class="[
      'workspace-sidebar-control',
      props.collapsed
        ? 'workspace-sidebar-control-square'
        : 'workspace-sidebar-control-inline gap-3',
    ]"
  >
    <div
      class="workspace-sidebar-brand-mark flex shrink-0 items-center justify-center"
    >
      <UIcon name="i-lucide-shield-check" class="size-5" />
    </div>

    <div v-if="!props.collapsed" class="min-w-0 space-y-0.5">
      <div class="truncate text-sm font-semibold tracking-tight text-highlighted">Hitomi</div>
      <div class="truncate text-xs text-muted">
        考勤后台
      </div>
    </div>
  </NuxtLink>
</template>
