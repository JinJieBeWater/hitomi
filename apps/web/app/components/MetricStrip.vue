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
  if (color === "primary") return "border-amber-400/70 bg-amber-500/12 text-amber-700 dark:text-amber-300";
  if (color === "success") return "border-neutral-400/70 bg-[var(--workspace-panel)] text-neutral-700 dark:border-neutral-700 dark:text-neutral-300";
  if (color === "warning") return "border-neutral-400/70 bg-[var(--workspace-panel)] text-neutral-700 dark:border-neutral-700 dark:text-neutral-300";
  if (color === "error") return "border-rose-500/40 bg-rose-500/10 text-rose-700 dark:text-rose-300";
  if (color === "neutral") return "border-neutral-400/70 bg-[var(--workspace-panel)] text-neutral-700 dark:border-neutral-700 dark:text-neutral-300";

  return "border-amber-400/70 bg-amber-500/12 text-amber-700 dark:text-amber-300";
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
          'flex size-11 shrink-0 items-center justify-center border',
          toneClass(item.color),
        ]"
      >
        <UIcon :name="item.icon" class="size-5" />
      </div>
    </div>
  </div>
</template>
