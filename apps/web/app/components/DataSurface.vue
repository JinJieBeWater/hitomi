<script setup lang="ts">
defineProps<{
  title: string;
  subtitle?: string;
  icon?: string;
}>();
</script>

<template>
  <section class="workspace-surface">
    <div class="workspace-surface-header">
      <div class="flex min-w-0 items-start gap-3">
        <div
          v-if="icon"
          class="mt-0.5 flex h-9 w-9 shrink-0 items-center justify-center rounded-2xl bg-primary/10 text-primary"
        >
          <UIcon :name="icon" class="h-4 w-4" />
        </div>
        <div class="min-w-0">
          <h2 class="text-base font-semibold tracking-tight text-highlighted">{{ title }}</h2>
          <p v-if="subtitle" class="mt-1 text-sm text-toned">{{ subtitle }}</p>
        </div>
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
