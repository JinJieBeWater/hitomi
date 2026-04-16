<script setup lang="ts">
import ThemeToggle from "~/components/ThemeToggle.vue";
</script>

<template>
  <div class="min-h-screen bg-transparent">
    <div class="fixed top-4 right-4 z-20 sm:top-6 sm:right-6">
      <ThemeToggle compact />
    </div>

    <UMain>
      <slot />
    </UMain>
  </div>
</template>
