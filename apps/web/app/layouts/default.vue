<script setup lang="ts">
import ThemeToggle from "~/components/ThemeToggle.vue";
</script>

<template>
  <div
    class="min-h-screen bg-[radial-gradient(circle_at_top_left,rgba(16,185,129,0.08),transparent_22%)]"
  >
    <div class="fixed top-4 right-4 z-20 sm:top-6 sm:right-6">
      <ThemeToggle compact class="shadow-sm backdrop-blur-sm" />
    </div>

    <UMain>
      <slot />
    </UMain>
  </div>
</template>
