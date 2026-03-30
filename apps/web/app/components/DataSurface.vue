<script setup lang="ts">
defineProps<{
  title: string;
}>();
</script>

<template>
  <section class="workspace-surface">
    <div class="workspace-surface-header">
      <div class="min-w-0">
        <h2 class="text-base font-semibold tracking-tight text-highlighted">{{ title }}</h2>
      </div>

      <div v-if="$slots.actions" class="flex flex-wrap gap-2">
        <slot name="actions" />
      </div>
    </div>

    <div class="workspace-surface-body">
      <slot />
    </div>

    <div v-if="$slots.footer" class="workspace-surface-footer">
      <slot name="footer" />
    </div>
  </section>
</template>
