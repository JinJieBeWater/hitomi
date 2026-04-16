<script setup lang="ts">
withDefaults(
  defineProps<{
    title: string;
    description?: string;
    icon?: string;
  }>(),
  {
    description: "",
  },
);
</script>

<template>
  <div class="workspace-empty-state">
    <div
      v-if="icon"
      class="workspace-panel-icon flex size-12 items-center justify-center text-neutral-500"
    >
      <UIcon :name="icon" class="size-6" />
    </div>

    <div class="space-y-1">
      <div class="text-base font-semibold tracking-tight text-highlighted">{{ title }}</div>
      <div v-if="description" class="text-sm text-toned">{{ description }}</div>
    </div>

    <div v-if="$slots.actions" class="flex flex-wrap gap-2">
      <slot name="actions" />
    </div>
  </div>
</template>
