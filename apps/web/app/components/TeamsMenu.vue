<script setup lang="ts">
const props = defineProps<{
  collapsed?: boolean;
}>();
</script>

<template>
  <NuxtLink
    to="/dashboard"
    class="group flex w-full items-center rounded-2xl px-2 py-2 transition-colors hover:bg-white/70 dark:hover:bg-neutral-900/70"
    :class="props.collapsed ? 'justify-center' : 'gap-3'"
  >
    <div
      class="flex shrink-0 items-center justify-center rounded-2xl bg-neutral-950 text-white shadow-sm ring-1 ring-inset ring-white/10 dark:bg-white dark:text-neutral-950"
      :class="props.collapsed ? 'size-11' : 'size-10'"
    >
      <UIcon name="i-lucide-shield-check" :class="props.collapsed ? 'size-5.5' : 'size-5'" />
    </div>

    <div v-if="!props.collapsed" class="min-w-0 space-y-0.5">
      <div class="truncate text-sm font-semibold tracking-tight text-highlighted">Hitomi</div>
      <div class="truncate text-[11px] font-medium tracking-[0.16em] text-muted uppercase">
        考勤后台
      </div>
    </div>
  </NuxtLink>
</template>
