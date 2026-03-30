<script setup lang="ts">
withDefaults(
  defineProps<{
    title: string;
    description?: string;
    icon?: string;
  }>(),
  {
    description: "",
    icon: "i-lucide-inbox",
  },
);
</script>

<template>
  <div class="workspace-empty-state">
    <div class="flex size-12 items-center justify-center rounded-2xl bg-neutral-900/5 text-neutral-500 ring-1 ring-inset ring-neutral-900/10">
      <UIcon :name="icon" class="size-6" />
    </div>

    <div class="space-y-1">
      <div class="text-base font-semibold tracking-tight text-highlighted">{{ title }}</div>
      <div v-if="description" class="text-sm text-toned">{{ description }}</div>
    </div>

    <div v-if="$slots.actions" class="flex flex-wrap justify-center gap-2">
      <slot name="actions" />
    </div>
  </div>
</template>
