<script setup lang="ts">
import { useQuery } from "@tanstack/vue-query";

definePageMeta({
  layout: "dashboard",
  middleware: ["auth"],
});

const { $orpc } = useNuxtApp();

const summary = useQuery($orpc.dashboard.summary.queryOptions());
const summaryData = computed(() => summary.data.value);

const metrics = computed(() => {
  const data = summary.data.value;

  return [
    {
      label: "员工总数",
      value: data?.employeeCount ?? 0,
      icon: "i-lucide-users",
      color: "primary" as const,
    },
    {
      label: "设备总数",
      value: data?.deviceCount ?? 0,
      icon: "i-lucide-monitor-smartphone",
      color: "success" as const,
    },
    {
      label: "今日上班打卡",
      value: data?.todayClockInCount ?? 0,
      icon: "i-lucide-sunrise",
      color: "warning" as const,
    },
    {
      label: "今日下班打卡",
      value: data?.todayClockOutCount ?? 0,
      icon: "i-lucide-sunset",
      color: "neutral" as const,
    },
  ];
});

const shortcuts = [
  { label: "员工管理", to: "/employees", icon: "i-lucide-users" },
  { label: "设备管理", to: "/devices", icon: "i-lucide-monitor-smartphone" },
  { label: "录脸记录", to: "/face-profiles", icon: "i-lucide-scan-face" },
  { label: "考勤记录", to: "/attendance-records", icon: "i-lucide-clipboard-check" },
];
</script>

<template>
  <UDashboardPanel id="dashboard">
    <template #header>
      <PageHeader
        title="概览"
        :badges="
          summaryData?.todayLocalDate
            ? [{ label: summaryData.todayLocalDate, color: 'neutral' }]
            : []
        "
      >
        <template #actions>
          <UButton variant="outline" icon="i-lucide-refresh-cw" @click="summary.refetch()"
            >刷新</UButton
          >
        </template>
      </PageHeader>
    </template>

    <template #body>
      <div class="workspace-page-stack">
        <MetricStrip :metrics="metrics" />

        <UAlert
          v-if="summary.status.value === 'error'"
          color="error"
          icon="i-lucide-alert-circle"
          title="加载失败"
          :description="summary.error.value?.message || '无法加载统计信息'"
        />

        <AttendanceConfigEditor />

        <DataSurface title="快捷操作">
          <div class="grid gap-3 md:grid-cols-2 xl:grid-cols-4">
            <UButton
              v-for="item in shortcuts"
              :key="item.to"
              :to="item.to"
              color="neutral"
              variant="outline"
              class="h-12 justify-between"
              :icon="item.icon"
            >
              {{ item.label }}
            </UButton>
          </div>
        </DataSurface>
      </div>
    </template>
  </UDashboardPanel>
</template>
