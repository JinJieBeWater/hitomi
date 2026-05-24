<script setup lang="ts">
import { useMutation, useQueryClient } from "@tanstack/vue-query";
import { ref, watch } from "vue";

type CreatedResult = {
  id: string;
  deviceCode: string;
  bootstrapSerial: string;
  bootstrapSecret: string;
  name: string;
};

const props = defineProps<{
  open: boolean;
  device?: { id: string; name: string; status: string } | null;
}>();

const emit = defineEmits<{
  "update:open": [value: boolean];
  saved: [];
  created: [result: CreatedResult];
  "request-delete": [];
}>();

const { $orpc } = useNuxtApp();
const queryClient = useQueryClient();
const toast = useToast();

const form = ref({ name: "", status: "active" });
const editorError = ref("");

const createDevice = useMutation($orpc.device.create.mutationOptions());
const updateDevice = useMutation($orpc.device.update.mutationOptions());

const isSaving = computed(
  () => createDevice.isPending.value || updateDevice.isPending.value,
);

const deviceStatusOptions = [
  { label: "启用", value: "active", description: "设备可继续上报业务数据" },
  { label: "禁用", value: "disabled", description: "设备被停用，但历史数据保留" },
];

watch(
  [() => props.open, () => props.device],
  ([open, device]) => {
    if (!open) {
      form.value = { name: "", status: "active" };
      editorError.value = "";
    } else if (device) {
      form.value = { name: device.name, status: device.status };
    } else {
      form.value = { name: "", status: "active" };
    }
  },
);

async function handleSubmit() {
  editorError.value = "";

  try {
    if (props.device) {
      await updateDevice.mutateAsync({
        id: props.device.id,
        name: form.value.name.trim(),
        status: form.value.status as "active" | "disabled",
      });

      await queryClient.invalidateQueries();
      emit("update:open", false);
      toast.add({ title: "设备已更新" });
    } else {
      const result = await createDevice.mutateAsync({
        name: form.value.name.trim(),
      });

      await queryClient.invalidateQueries();
      emit("update:open", false);
      emit("created", {
        id: result.device.id,
        name: result.device.name,
        deviceCode: result.device.deviceCode,
        bootstrapSerial: result.bootstrapSerial,
        bootstrapSecret: result.bootstrapSecret,
      });
    }

    emit("saved");
  } catch (error: any) {
    editorError.value = error?.message || "保存失败";
  }
}
</script>

<template>
  <USlideover
    :open="open"
    :title="device ? '编辑设备' : '创建设备'"
    side="right"
    @update:open="emit('update:open', $event)"
  >
    <template #body>
      <form class="flex flex-col gap-4" @submit.prevent="handleSubmit">
        <div class="rounded-lg border border-default bg-default p-4">
          <div class="workspace-section-label">设备配置</div>
          <div class="workspace-data-value">{{ device ? "在线终端维护" : "注册新终端" }}</div>
          <div class="mt-1 text-sm text-toned">
            {{ device ? "修改名称或启停状态，不改动历史凭据。" : "创建后会生成 bootstrap 凭据并进入首配流程。" }}
          </div>
        </div>

        <UFormField label="设备名称" required>
          <UInput
            v-model="form.name"
            data-testid="device-name-input"
            placeholder="例如 前台一号机"
            icon="i-lucide-monitor"
            class="w-full"
          />
        </UFormField>

        <UFormField v-if="device" label="状态">
          <USelect
            v-model="form.status"
            data-testid="device-status-select"
            :items="deviceStatusOptions"
            class="w-full"
          />
        </UFormField>

        <UAlert
          v-if="editorError"
          color="error"
          icon="i-lucide-alert-circle"
          title="操作失败"
          :description="editorError"
        />

        <div class="flex flex-col gap-3 pt-2">
          <UButton
            type="submit"
            data-testid="device-submit-button"
            :loading="isSaving"
            class="w-full"
            icon="i-lucide-save"
          >
            {{ device ? "保存修改" : "创建设备" }}
          </UButton>

          <UButton
            type="button"
            variant="outline"
            color="neutral"
            class="w-full"
            @click="emit('update:open', false)"
            >取消</UButton
          >
        </div>

        <div v-if="device" class="workspace-danger-panel space-y-3">
          <div>
            <div class="workspace-section-label text-rose-600 dark:text-rose-300">危险操作</div>
            <div class="mt-1 text-sm text-toned">删除设备会一并清理其相关录脸任务和考勤记录。</div>
          </div>

          <UButton
            type="button"
            color="error"
            variant="outline"
            class="w-full"
            icon="i-lucide-trash-2"
            @click="emit('request-delete')"
          >
            删除设备
          </UButton>
        </div>
      </form>
    </template>
  </USlideover>
</template>
