<script setup lang="ts">
type BadgeTone = "primary" | "success" | "warning" | "error" | "neutral";

const props = withDefaults(
  defineProps<{
    title: string;
    badges?: Array<{
      label: string;
      color?: BadgeTone;
    }>;
  }>(),
  {
    badges: () => [],
  },
);
</script>

<template>
  <div>
    <UDashboardNavbar :title="props.title" :ui="{ right: 'gap-2' }">
      <template #leading>
        <UDashboardSidebarCollapse />
      </template>

      <template #right>
        <slot name="actions" />
      </template>
    </UDashboardNavbar>

    <UDashboardToolbar v-if="props.badges.length">
      <template #left>
        <div class="flex flex-wrap items-center gap-2">
          <UBadge
            v-for="item in props.badges"
            :key="item.label"
            :label="item.label"
            :color="item.color || 'neutral'"
            variant="subtle"
            class="rounded-full"
          />
        </div>
      </template>
    </UDashboardToolbar>
  </div>
</template>
