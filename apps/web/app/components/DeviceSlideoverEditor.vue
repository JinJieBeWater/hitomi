<script setup lang="ts">
import { useMutation, useQueryClient } from "@tanstack/vue-query";
import { ref, watch } from "vue";

type CreatedResult = {
  id: string;
  deviceCode: string;
  initialApiKey: string;
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
        initialApiKey: result.initialApiKey,
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
      <form class="space-y-5" @submit.prevent="handleSubmit">
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
            class="w-full rounded-2xl"
            icon="i-lucide-save"
          >
            {{ device ? "保存修改" : "创建设备" }}
          </UButton>

          <UButton
            type="button"
            variant="ghost"
            color="neutral"
            class="w-full"
            @click="emit('update:open', false)"
            >取消</UButton
          >
        </div>

        <div
          v-if="device"
          class="border-t border-neutral-200/70 pt-5 dark:border-neutral-800/80"
        >
          <UButton
            type="button"
            color="error"
            variant="outline"
            class="w-full rounded-2xl"
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
