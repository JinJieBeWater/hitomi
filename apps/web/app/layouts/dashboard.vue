<script setup lang="ts">
import ThemeToggle from "~/components/ThemeToggle.vue";
import UserMenu from "~/components/UserMenu.vue";

const open = ref(false);

function closeSidebar(): void {
  open.value = false;
}

function createLink(label: string, icon: string, to: string) {
  return {
    label,
    icon,
    to,
    onSelect: closeSidebar,
  };
}

const mainLinks = [
  createLink("概览", "i-lucide-layout-dashboard", "/dashboard"),
  createLink("员工管理", "i-lucide-users", "/employees"),
  createLink("设备管理", "i-lucide-monitor-smartphone", "/devices"),
  createLink("录脸记录", "i-lucide-scan-face", "/face-profiles"),
  createLink("考勤记录", "i-lucide-clipboard-check", "/attendance-records"),
];

const groups = [
  {
    id: "pages",
    label: "页面",
    items: mainLinks.map((item) => ({
      id: item.to,
      label: item.label,
      icon: item.icon,
      to: item.to,
      onSelect: item.onSelect,
    })),
  },
];

function sidebarControlClass(collapsed: boolean): string {
  return collapsed
    ? "workspace-sidebar-control workspace-sidebar-control-square"
    : "workspace-sidebar-control workspace-sidebar-control-inline";
}

function sidebarNavLinkClass(collapsed: boolean): string {
  return collapsed
    ? "workspace-sidebar-nav-link workspace-sidebar-nav-link-collapsed"
    : "workspace-sidebar-nav-link";
}
</script>

<template>
  <UDashboardGroup storage-key="hitomi-dashboard" unit="rem" class="w-full min-w-0">
    <UDashboardSidebar
      id="hitomi"
      v-model:open="open"
      mode="slideover"
      collapsible
      resizable
      class="border-r border-default bg-default"
      :ui="{
        header: 'px-3 pt-3',
        body: 'px-3 py-2',
        footer: 'px-3 py-3 lg:border-t lg:border-neutral-400/70 dark:lg:border-neutral-700/80',
      }"
    >
      <template #header="{ collapsed }">
        <div :class="collapsed ? 'flex w-full justify-center' : 'w-full'">
          <TeamsMenu :collapsed="collapsed" />
        </div>
      </template>

      <template #default="{ collapsed }">
        <div :class="['flex flex-col gap-2', collapsed ? 'items-center' : '']">
          <UDashboardSearchButton
            :collapsed="collapsed"
            variant="outline"
            color="neutral"
            size="lg"
            :class="sidebarControlClass(collapsed)"
            :ui="{
              base: 'ring-0 shadow-none',
              label: 'workspace-sidebar-nav-label',
              trailing: collapsed ? 'hidden' : 'hidden lg:flex items-center gap-0.5 ms-auto',
            }"
          />

          <UNavigationMenu
            :collapsed="collapsed"
            :items="mainLinks"
            orientation="vertical"
            color="neutral"
            variant="pill"
            highlight
            highlight-color="primary"
            tooltip
            popover
            :ui="{
              root: collapsed ? 'flex flex-col items-center gap-2' : 'flex flex-col gap-2',
              list: collapsed ? 'flex flex-col items-center gap-2' : 'flex flex-col gap-2',
              item: collapsed ? 'flex justify-center' : '',
              link: `${sidebarNavLinkClass(collapsed)} transition-colors`,
              label: 'workspace-sidebar-nav-label',
              linkLeadingIcon: collapsed ? 'size-5' : 'size-5',
            }"
          />
        </div>
      </template>

      <template #footer="{ collapsed }">
        <div :class="['flex w-full min-w-0 flex-col gap-2', collapsed ? 'items-center' : '']">
          <div :class="collapsed ? 'flex w-full justify-center' : 'w-full'">
            <ThemeToggle :collapsed="collapsed" />
          </div>
          <div :class="collapsed ? 'flex w-full justify-center' : 'w-full'">
            <UserMenu :collapsed="collapsed" />
          </div>
        </div>
      </template>
    </UDashboardSidebar>

    <UDashboardSearch :groups="groups" />

    <slot />
  </UDashboardGroup>
</template>
