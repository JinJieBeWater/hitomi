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
    @update:open="emit('update:open', $event)"
  >
    <template #body>
      <form class="space-y-5" @submit.prevent="handleSubmit">
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
            class="w-full rounded-2xl"
            icon="i-lucide-save"
          >
            {{ employee ? "保存修改" : "创建员工" }}
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
          v-if="employee"
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
            删除员工
          </UButton>
        </div>
      </form>
    </template>
  </USlideover>
</template>
