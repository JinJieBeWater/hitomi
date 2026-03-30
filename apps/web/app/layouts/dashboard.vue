<script setup lang="ts">
import type { NavigationMenuItem } from "@nuxt/ui";

import ThemeToggle from "~/components/ThemeToggle.vue";
import UserMenu from "~/components/UserMenu.vue";

const open = ref(false);

function closeSidebar(): void {
  open.value = false;
}

function createLink(label: string, icon: string, to: string): NavigationMenuItem {
  return {
    label,
    icon,
    to,
    onSelect: closeSidebar,
  };
}

const mainLinks: NavigationMenuItem[] = [
  createLink("概览", "i-lucide-layout-dashboard", "/dashboard"),
  createLink("员工管理", "i-lucide-users", "/employees"),
  createLink("设备管理", "i-lucide-monitor-smartphone", "/devices"),
  createLink("考勤配置", "i-lucide-settings-2", "/attendance-config"),
  createLink("录脸任务", "i-lucide-scan-face", "/face-profiles"),
  createLink("考勤记录", "i-lucide-clipboard-check", "/attendance-records"),
];

const groups = [
  {
    id: "pages",
    label: "页面",
    items: mainLinks,
  },
];
</script>

<template>
  <UDashboardGroup storage-key="hitomi-dashboard" unit="rem" class="w-full min-w-0">
    <UDashboardSidebar
      id="hitomi"
      v-model:open="open"
      mode="slideover"
      collapsible
      resizable
      class="border-r border-neutral-200/70 bg-white/72 backdrop-blur dark:border-neutral-800/80 dark:bg-neutral-950/85"
      :ui="{
        header: 'px-3 pt-3',
        body: 'px-3 py-2',
        footer: 'px-3 py-3 lg:border-t lg:border-neutral-200/80 dark:lg:border-neutral-800/80',
      }"
    >
      <template #header="{ collapsed }">
        <TeamsMenu :collapsed="collapsed" />
      </template>

      <template #default="{ collapsed }">
        <UDashboardSearchButton
          :collapsed="collapsed"
          class="rounded-2xl border border-neutral-200/80 bg-white/78 shadow-sm ring-0 dark:border-neutral-800/80 dark:bg-neutral-900/80"
        />

        <UNavigationMenu
          :collapsed="collapsed"
          :items="mainLinks"
          orientation="vertical"
          color="neutral"
          variant="ghost"
          highlight
          highlight-color="primary"
          tooltip
          popover
          :ui="{
            link: 'rounded-2xl px-3 py-2.5 transition-colors',
            label: 'text-sm font-medium',
          }"
        />
      </template>

      <template #footer="{ collapsed }">
        <div class="flex w-full min-w-0 flex-col gap-2">
          <ThemeToggle :collapsed="collapsed" />
          <UserMenu :collapsed="collapsed" />
        </div>
      </template>
    </UDashboardSidebar>

    <UDashboardSearch :groups="groups" />

    <slot />
  </UDashboardGroup>
</template>
