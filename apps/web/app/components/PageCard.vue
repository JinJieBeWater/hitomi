<script setup lang="ts">
defineProps<{
  title: string;
  subtitle?: string;
  icon?: string;
}>();
</script>

<template>
  <UCard>
    <template #header>
      <div class="flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between">
      <div class="flex min-w-0 items-start gap-3">
        <div
          v-if="icon"
          class="workspace-panel-icon mt-0.5 flex size-9 shrink-0 items-center justify-center"
        >
          <UIcon :name="icon" class="size-4" />
        </div>

        <div class="min-w-0">
          <h2 class="text-base font-semibold tracking-tight text-highlighted">
            {{ title }}
          </h2>
          <p v-if="subtitle" class="mt-1 text-sm text-toned">
            {{ subtitle }}
          </p>
        </div>
      </div>

      <div v-if="$slots.actions" class="flex flex-wrap gap-2">
        <slot name="actions" />
      </div>
      </div>
    </template>

    <slot />

    <template v-if="$slots.footer" #footer>
      <slot name="footer" />
    </template>
  </UCard>
</template>
