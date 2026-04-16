<script setup lang="ts">
const props = withDefaults(
  defineProps<{
    collapsed?: boolean;
    compact?: boolean;
  }>(),
  {
    collapsed: false,
    compact: false,
  },
);

const colorMode = useColorMode();

const nextPreference = computed(() => (colorMode.value === "dark" ? "light" : "dark"));
const actionLabel = computed(() => (nextPreference.value === "dark" ? "切换到深色" : "切换到浅色"));
const actionIcon = computed(() =>
  nextPreference.value === "dark" ? "i-lucide-moon" : "i-lucide-sun-medium",
);
const showLabel = computed(() => !props.collapsed && !props.compact);

function toggleTheme() {
  colorMode.preference = nextPreference.value;
}
</script>

<template>
  <UButton
    color="neutral"
    variant="outline"
    size="lg"
    :square="props.collapsed || props.compact"
    :block="!props.collapsed && !props.compact"
    :icon="actionIcon"
    :label="showLabel ? actionLabel : undefined"
    :title="actionLabel"
    :aria-label="actionLabel"
    :class="
      props.collapsed || props.compact
        ? 'workspace-sidebar-control workspace-sidebar-control-square'
        : 'workspace-sidebar-control workspace-sidebar-control-inline workspace-sidebar-nav-label'
    "
    @click="toggleTheme()"
  />
</template>
