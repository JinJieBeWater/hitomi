<script setup lang="ts">
import type { FormSubmitEvent, AuthFormField } from "@nuxt/ui";
import * as z from "zod";

const { $authClient } = useNuxtApp();

const toast = useToast();
const loading = ref(false);

const fields: AuthFormField[] = [
  {
    name: "name",
    type: "text",
    label: "姓名",
    placeholder: "请输入姓名",
    required: true,
  },
  {
    name: "email",
    type: "email",
    label: "邮箱",
    placeholder: "请输入邮箱",
    required: true,
  },
  {
    name: "password",
    type: "password",
    label: "密码",
    placeholder: "请输入密码",
    required: true,
  },
];

const schema = z.object({
  name: z.string().min(2, "姓名至少需要 2 个字符"),
  email: z.email("请输入有效的邮箱地址"),
  password: z.string().min(8, "密码至少需要 8 位"),
});

type Schema = z.output<typeof schema>;

async function onSubmit(event: FormSubmitEvent<Schema>) {
  loading.value = true;
  try {
    await $authClient.signUp.email(
      {
        name: event.data.name,
        email: event.data.email,
        password: event.data.password,
      },
      {
        onSuccess: async () => {
          toast.add({ title: "注册成功" });
          await navigateTo("/dashboard", { replace: true, external: true });
        },
        onError: (error) => {
          toast.add({ title: "注册失败", description: error.error.message });
        },
      },
    );
  } catch (error: any) {
    toast.add({
      title: "发生未预期错误",
      description: error.message || "请稍后重试。",
    });
  } finally {
    loading.value = false;
  }
}
</script>

<template>
  <UPageCard class="workspace-auth-card w-full">
    <UAuthForm
      :schema="schema"
      :fields="fields"
      title="管理员注册"
      icon="i-lucide-user-plus"
      :submit="{ label: '注册', loading }"
      @submit="onSubmit"
    >
      <template #description>
        已有账号？
        <ULink class="font-medium text-primary" @click="$emit('switchToSignIn')"> 去登录 </ULink>
      </template>
    </UAuthForm>
  </UPageCard>
</template>
