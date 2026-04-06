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

const hiddenCaptions = new Set(["当前页", "当前筛选"]);

function toneClass(color?: MetricTone) {
  if (color === "primary") return "bg-primary/10 text-primary ring-primary/20";
  if (color === "success") return "bg-emerald-500/10 text-emerald-700 ring-emerald-500/20";
  if (color === "warning") return "bg-amber-500/10 text-amber-700 ring-amber-500/20";
  if (color === "error") return "bg-rose-500/10 text-rose-700 ring-rose-500/20";
  if (color === "neutral") return "bg-neutral-900/5 text-neutral-600 ring-neutral-900/10";

  return "bg-primary/10 text-primary ring-primary/20";
}

function shouldShowCaption(caption?: string): boolean {
  return typeof caption === "string" && caption.length > 0 && !hiddenCaptions.has(caption);
}
</script>

<template>
  <div class="workspace-metric-grid">
    <div v-for="item in props.metrics" :key="item.label" class="workspace-metric-item">
      <div class="min-w-0 flex-1 space-y-1">
        <div class="workspace-metric-label">{{ item.label }}</div>
        <div class="workspace-metric-value">{{ item.value }}</div>
        <div v-if="shouldShowCaption(item.caption)" class="workspace-metric-meta">
          {{ item.caption }}
        </div>
      </div>

      <div
        v-if="item.icon"
        :class="[
          'flex size-11 shrink-0 items-center justify-center rounded-2xl ring-1 ring-inset',
          toneClass(item.color),
        ]"
      >
        <UIcon :name="item.icon" class="size-5" />
      </div>
    </div>
  </div>
</template>
