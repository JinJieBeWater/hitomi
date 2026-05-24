<script setup lang="ts">
import type { DropdownMenuItem } from "@nuxt/ui";

const props = withDefaults(
  defineProps<{
    items: DropdownMenuItem[][];
    triggerSize?: "xs" | "sm";
  }>(),
  {
    triggerSize: "xs",
  },
);
</script>

<template>
  <UDropdownMenu
    :items="props.items"
    :ui="{ content: 'min-w-40' }"
  >
    <UButton
      :size="triggerSize"
      color="neutral"
      variant="outline"
      icon="i-lucide-ellipsis-vertical"
      aria-label="更多操作"
    />
  </UDropdownMenu>
</template>
