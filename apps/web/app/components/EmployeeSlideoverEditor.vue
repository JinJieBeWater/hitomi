<script setup lang="ts">
import { useMutation, useQueryClient } from "@tanstack/vue-query";
import { ref, watch } from "vue";

const props = defineProps<{
  open: boolean;
  employee?: { id: string; code: string; name: string } | null;
}>();

const emit = defineEmits<{
  "update:open": [value: boolean];
  saved: [];
  "request-delete": [];
}>();

const { $orpc } = useNuxtApp();
const queryClient = useQueryClient();
const toast = useToast();

const form = ref({ code: "", name: "" });
const editorError = ref("");

const createEmployee = useMutation($orpc.employee.create.mutationOptions());
const updateEmployee = useMutation($orpc.employee.update.mutationOptions());

const isSaving = computed(
  () => createEmployee.isPending.value || updateEmployee.isPending.value,
);

watch(
  [() => props.open, () => props.employee],
  ([open, employee]) => {
    if (!open) {
      form.value = { code: "", name: "" };
      editorError.value = "";
    } else if (employee) {
      form.value = { code: employee.code, name: employee.name };
    } else {
      form.value = { code: "", name: "" };
    }
  },
);

async function handleSubmit() {
  editorError.value = "";
  const isEditing = Boolean(props.employee);

  try {
    if (props.employee) {
      await updateEmployee.mutateAsync({
        id: props.employee.id,
        code: form.value.code.trim(),
        name: form.value.name.trim(),
      });
    } else {
      await createEmployee.mutateAsync({
        code: form.value.code.trim(),
        name: form.value.name.trim(),
      });
    }

    await queryClient.invalidateQueries();
    emit("update:open", false);
    emit("saved");
    toast.add({
      title: isEditing ? "员工已更新" : "员工已创建",
    });
  } catch (error: any) {
    editorError.value = error?.message || "保存失败";
  }
}
</script>

<template>
  <USlideover
    :open="open"
    :title="employee ? '编辑员工' : '新增员工'"
    side="right"
    :ui="{
      content: 'workspace-dialog-content',
      header: 'workspace-dialog-header',
      body: 'workspace-dialog-body',
      footer: 'workspace-dialog-footer',
      title: 'workspace-dialog-title',
      description: 'workspace-dialog-description',
      close: 'workspace-dialog-close',
    }"
    @update:open="emit('update:open', $event)"
  >
    <template #body>
      <form class="workspace-dialog-stack" @submit.prevent="handleSubmit">
        <div class="workspace-dialog-panel">
          <div class="workspace-section-label">维护对象</div>
          <div class="workspace-data-value">{{ employee ? "员工档案" : "新建员工" }}</div>
          <div class="mt-1 text-sm text-toned">
            {{ employee ? "更新编号与姓名，保持设备侧识别信息一致。" : "创建后可继续分配录脸任务。" }}
          </div>
        </div>

        <UFormField label="员工编号" required>
          <UInput
            v-model="form.code"
            data-testid="employee-code-input"
            placeholder="例如 E1001"
            icon="i-lucide-id-card"
            class="w-full"
          />
        </UFormField>

        <UFormField label="员工姓名" required>
          <UInput
            v-model="form.name"
            data-testid="employee-name-input"
            placeholder="请输入员工姓名"
            icon="i-lucide-user"
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
            data-testid="employee-submit-button"
            :loading="isSaving"
            class="workspace-primary-action w-full"
            icon="i-lucide-save"
          >
            {{ employee ? "保存修改" : "创建员工" }}
          </UButton>

          <UButton
            type="button"
            variant="outline"
            color="neutral"
            class="workspace-secondary-action w-full"
            @click="emit('update:open', false)"
            >取消</UButton
          >
        </div>

        <div v-if="employee" class="workspace-danger-panel space-y-3">
          <div>
            <div class="workspace-section-label text-rose-600 dark:text-rose-300">危险操作</div>
            <div class="mt-1 text-sm text-toned">删除员工会连带移除录脸任务和考勤记录。</div>
          </div>

          <UButton
            type="button"
            color="error"
            variant="outline"
            class="workspace-secondary-action w-full"
            icon="i-lucide-trash-2"
            @click="emit('request-delete')"
          >
            删除员工
          </UButton>
        </div>
      </form>
    </template>
  </USlideover>
</template>
