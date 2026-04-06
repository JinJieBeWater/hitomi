<script setup lang="ts">
import { useMutation, useQueryClient } from "@tanstack/vue-query";
import { computed, ref, watch } from "vue";

import { colorFaceStatus, labelFaceStatus } from "~/utils/format";

type FaceProfile = {
  id: string;
  status: string;
  deviceId?: string | null;
  deviceName?: string | null;
};

type Employee = {
  id: string;
  name: string;
  code: string;
  faceProfile?: FaceProfile | null;
};

const props = defineProps<{
  open: boolean;
  employee: Employee | null;
  deviceOptions: Array<{ label: string; value: string; description: string }>;
  deviceOptionsLoading?: boolean;
}>();

const emit = defineEmits<{
  "update:open": [value: boolean];
  saved: [];
}>();

const { $orpc } = useNuxtApp();
const queryClient = useQueryClient();
const toast = useToast();

const deviceId = ref<string | undefined>();
const faceTaskError = ref("");

const assignFaceTask = useMutation($orpc.faceProfile.enqueue.mutationOptions());
const cancelFaceTask = useMutation($orpc.faceProfile.cancel.mutationOptions());

const title = computed(() => {
  if (!props.employee) return "录脸任务";
  return `${props.employee.name} · 录脸`;
});

const actionLabel = computed(() => {
  const faceStatus = props.employee?.faceProfile?.status;

  if (!faceStatus || faceStatus === "cancelled" || faceStatus === "failed") {
    return "创建录脸任务";
  }

  if (faceStatus === "pending") {
    return "更新设备分配";
  }

  return "重新发起录脸";
});

const canCancel = computed(() => props.employee?.faceProfile?.status === "pending");

watch(
  [() => props.open, () => props.employee],
  ([open, employee]) => {
    if (!open) {
      deviceId.value = undefined;
      faceTaskError.value = "";
    } else {
      deviceId.value = employee?.faceProfile?.deviceId || undefined;
    }
  },
);

async function handleSubmit() {
  faceTaskError.value = "";

  if (!props.employee) return;

  if (!deviceId.value) {
    faceTaskError.value = "请选择设备";
    return;
  }

  const hadTask = Boolean(props.employee.faceProfile);

  try {
    await assignFaceTask.mutateAsync({
      employeeId: props.employee.id,
      deviceId: deviceId.value,
    });

    await queryClient.invalidateQueries();
    emit("update:open", false);
    emit("saved");
    toast.add({
      title: hadTask ? "录脸任务已更新" : "录脸任务已创建",
    });
  } catch (error: any) {
    faceTaskError.value = error?.message || "录脸任务保存失败";
  }
}

async function handleCancel() {
  faceTaskError.value = "";

  const profile = props.employee?.faceProfile;
  if (!profile) return;

  try {
    await cancelFaceTask.mutateAsync({ id: profile.id });
    await queryClient.invalidateQueries();
    emit("update:open", false);
    emit("saved");
    toast.add({ title: "录脸任务已取消" });
  } catch (error: any) {
    faceTaskError.value = error?.message || "取消录脸任务失败";
  }
}
</script>

<template>
  <USlideover :open="open" :title="title" side="right" @update:open="emit('update:open', $event)">
    <template #body>
      <div v-if="employee" class="space-y-5">
        <div class="space-y-1">
          <div class="text-xs font-medium tracking-[0.14em] text-muted uppercase">员工</div>
          <div class="text-lg font-semibold tracking-tight text-highlighted">
            {{ employee.name }}
          </div>
          <div class="text-sm text-toned">{{ employee.code }}</div>
        </div>

        <div class="grid gap-4 sm:grid-cols-2">
          <div>
            <div class="text-xs font-medium tracking-[0.14em] text-muted uppercase">当前状态</div>
            <div class="mt-2">
              <UBadge
                :label="labelFaceStatus(employee.faceProfile?.status)"
                :color="colorFaceStatus(employee.faceProfile?.status)"
                variant="subtle"
                class="rounded-full"
              />
            </div>
          </div>

          <div>
            <div class="text-xs font-medium tracking-[0.14em] text-muted uppercase">当前设备</div>
            <div class="mt-2 text-sm font-medium text-highlighted">
              {{ employee.faceProfile?.deviceName || "未分配设备" }}
            </div>
          </div>
        </div>

        <UFormField label="录脸设备" required>
          <USelect
            v-model="deviceId"
            data-testid="employee-face-device-select"
            :items="deviceOptions"
            :loading="deviceOptionsLoading"
            placeholder="请选择设备"
            class="w-full"
          />
        </UFormField>

        <UAlert
          v-if="!deviceOptionsLoading && deviceOptions.length === 0"
          color="warning"
          icon="i-lucide-triangle-alert"
          title="暂无可用设备"
          description="请先启用至少一台设备。"
        />

        <UAlert
          v-if="faceTaskError"
          color="error"
          icon="i-lucide-alert-circle"
          title="操作失败"
          :description="faceTaskError"
        />

        <div class="flex flex-col gap-3 pt-2">
          <UButton
            type="button"
            data-testid="employee-face-submit-button"
            :loading="assignFaceTask.isPending.value"
            :disabled="deviceOptions.length === 0"
            class="w-full rounded-2xl"
            icon="i-lucide-scan-face"
            @click="handleSubmit()"
          >
            {{ actionLabel }}
          </UButton>

          <UButton
            v-if="canCancel"
            type="button"
            color="error"
            variant="outline"
            :loading="cancelFaceTask.isPending.value"
            class="w-full rounded-2xl"
            icon="i-lucide-ban"
            @click="handleCancel()"
          >
            取消待录入
          </UButton>

          <UButton
            type="button"
            variant="ghost"
            color="neutral"
            class="w-full"
            @click="emit('update:open', false)"
            >关闭</UButton
          >
        </div>
      </div>
    </template>
  </USlideover>
</template>
