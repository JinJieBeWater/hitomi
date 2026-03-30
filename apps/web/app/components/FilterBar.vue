<script setup lang="ts">
withDefaults(
  defineProps<{
    sticky?: boolean;
  }>(),
  {
    sticky: true,
  },
);
</script>

<template>
  <div :class="['workspace-filter-bar', sticky ? 'lg:sticky lg:top-4 lg:z-10' : '']">
    <div class="flex flex-col gap-4 lg:flex-row lg:items-end lg:justify-between">
      <div class="min-w-0 flex-1">
        <slot />
      </div>

      <div v-if="$slots.actions" class="flex shrink-0 flex-wrap gap-2">
        <slot name="actions" />
      </div>
    </div>
  </div>
</template>
