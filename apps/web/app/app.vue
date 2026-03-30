<script setup lang="ts">
import { VueQueryDevtools } from "@tanstack/vue-query-devtools";

const colorMode = useColorMode();

useHead({
  meta: [
    { charset: "utf-8" },
    { name: "viewport", content: "width=device-width, initial-scale=1" },
    {
      key: "theme-color",
      name: "theme-color",
      content: computed(() => (colorMode.value === "dark" ? "#0a0a0a" : "#ffffff")),
    },
  ],
  link: [
    { rel: "icon", href: "/favicon.ico" },
  ],
  htmlAttrs: {
    lang: "zh-CN",
  },
});

const title = "Hitomi 考勤管理后台";
const description = "Hitomi 考勤管理后台。";

useSeoMeta({
  title,
  description,
  ogTitle: title,
  ogDescription: description,
  twitterCard: "summary_large_image",
});
</script>

<template>
  <UApp>
    <NuxtAnnouncer />
    <NuxtRouteAnnouncer />
    <NuxtLoadingIndicator />

    <NuxtLayout>
      <NuxtPage />
    </NuxtLayout>
  </UApp>
  <VueQueryDevtools />
</template>
