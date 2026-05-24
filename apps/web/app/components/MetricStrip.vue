<script setup lang="ts">
type MetricTone = "primary" | "success" | "warning" | "error" | "neutral";

const props = defineProps<{
  metrics: Array<{
    label: string;
    value: string | number;
    caption?: string;
    icon?: string;
    color?: MetricTone;
  }>;
}>();

function toneColor(color?: MetricTone) {
  return color ?? "primary";
}

function shouldShowCaption(caption?: string): boolean {
  return typeof caption === "string" && caption.length > 0;
}
</script>

<template>
  <div class="workspace-metric-grid">
    <UCard v-for="item in props.metrics" :key="item.label">
      <div class="flex items-start justify-between gap-4">
      <div class="min-w-0 flex-1 space-y-1">
        <div class="workspace-metric-label">{{ item.label }}</div>
        <div class="workspace-metric-value">{{ item.value }}</div>
        <div v-if="shouldShowCaption(item.caption)" class="workspace-metric-meta">
          {{ item.caption }}
        </div>
      </div>

      <UBadge v-if="item.icon" :color="toneColor(item.color)" variant="soft" size="lg" square>
        <UIcon :name="item.icon" class="size-5" />
      </UBadge>
      </div>
    </UCard>
  </div>
</template>
