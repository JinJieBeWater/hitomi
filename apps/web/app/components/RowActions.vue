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
    :ui="{
      content: 'workspace-dropdown-content min-w-40 p-1',
      item: 'workspace-dropdown-item',
      label: 'workspace-dropdown-label',
      separator: 'workspace-dropdown-separator',
      itemLabel: 'tracking-[0.08em]',
      itemDescription: 'normal-case tracking-normal text-xs',
    }"
  >
    <UButton
      :size="triggerSize"
      color="neutral"
      variant="outline"
      icon="i-lucide-ellipsis-vertical"
      class="workspace-secondary-action"
      aria-label="更多操作"
    />
  </UDropdownMenu>
</template>
