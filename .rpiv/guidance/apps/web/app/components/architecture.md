# apps/web/app/components — Vue 组件库

## Responsibility
Nuxt 管理后台的 23 个可复用 Vue 3 组件。所有组件使用 `<script setup lang="ts">`。Nuxt 自动导入，无需手动注册。通过 `useNuxtApp()` 获取 `$orpc`(oRPC) 和 `$authClient`(Better Auth)。数据获取与变更通过 `@tanstack/vue-query`。

## Dependencies
- **`@nuxt/ui` v4**: `UCard`, `UTable`, `UButton`, `UModal`, `USlideover`, `UBadge`, `UDropdownMenu`, `UPagination`, `UAuthForm` 等
- **`@orpc/tanstack-query`**: `$orpc.entity.list.queryOptions()`, `$orpc.entity.create.mutationOptions()`
- **`better-auth/vue`**: `$authClient.signIn.email()`, `$authClient.useSession()`

## Component Categories

### Layout/Card (5)
`PageHeader`, `DataSurface`, `EmptyState`, `QueryGuard`, `FilterBar`
- 注: `PageCard` **已废弃**，使用 `DataSurface` 代替（两者功能完全一致）

### Form/Editor (4)
`AttendanceConfigEditor`, `DeviceUsbConfigEditor`, `SignInForm`, `SignUpForm`

### Modal/Slideover (5)
`DeleteConfirmModal`, `DeviceSlideoverEditor`, `EmployeeSlideoverEditor`, `EmployeeFaceTaskSlideover`, `DeviceCredentialsModal`

### Data Display (3)
`MetricStrip`, `FilterBar`, `SerialLogTerminal`

### Navigation (3)
`TeamsMenu`, `UserMenu`, `ThemeToggle`

### Utility (3)
`RowActions`, `ListPagination`, `DeviceSerialWorkspace`

## Slideover Editor Pattern (canonical)

```vue
<script setup lang="ts">
const props = defineProps<{
  open: boolean;                              // true=显示, false=隐藏
  entity?: { id: string; name: string } | null;  // null=创建, non-null=编辑
}>();
const emit = defineEmits<{
  "update:open": [value: boolean];            // v-model 双向绑定
  saved: [];                                   // 保存成功信号
  "request-delete": [];                        // 委托父组件打开删除确认
}>();
const { $orpc } = useNuxtApp();
const queryClient = useQueryClient();
const toast = useToast();

const createMutation = useMutation($orpc.entity.create.mutationOptions());
const updateMutation = useMutation($orpc.entity.update.mutationOptions());

watch([() => props.open, () => props.entity], ([open, entity]) => {
  if (!open) { form.value = { name: "" }; return; }
  form.value = entity ? { name: entity.name } : { name: "" };
});

async function handleSubmit() {
  try {
    if (props.entity) await updateMutation.mutateAsync({ id: props.entity.id, name: form.value.name });
    else await createMutation.mutateAsync({ name: form.value.name });
    await queryClient.invalidateQueries();    // 刷新所有相关查询
    emit("update:open", false);               // 关闭面板
    emit("saved");
    toast.add({ title: props.entity ? "已更新" : "已创建" });
  } catch (error: any) { /* 显示行内错误 */ }
}
</script>

<template>
  <USlideover :open="open" side="right" :title="entity ? '编辑' : '新增'"
    @update:open="emit('update:open', $event)">
    <template #body>
      <form @submit.prevent="handleSubmit" class="flex flex-col gap-4">
        <!-- 表单字段 -->
        <UButton type="submit" :loading="isSaving" class="w-full">保存</UButton>
        <!-- 危险操作区域 (仅编辑模式): workspace-danger-panel -->
      </form>
    </template>
  </USlideover>
</template>
```

## DeleteConfirmModal Pattern

```vue
<!-- 使用方式: 父组件在打开 modal 前先查询删除影响范围 -->
<script setup lang="ts">
// 1. 获取删除影响 (受影响的人脸档案数、考勤记录数)
const impact = await queryClient.fetchQuery(
  $orpc.entity.getDeleteImpact.queryOptions({ input: { id: item.id } })
);
// 2. 将 impact 传给 modal — 包含 confirmText (用户必须精确输入此字符串才能删除)
// 3. 用户输入匹配后发出 confirm 事件 → 父组件调用 remove mutation
</script>
```

Modal 纯展示组件 — `loading`、`submitting`、`error`、`impact` 全由父组件通过 props 传入。

## QueryGuard — 四态守卫
所有数据列表的标准包装器。接管查询的 4 个状态:
- `status === "pending"` → 骨架屏 (USkeleton)
- `status === "error"` → 错误警告 (UAlert)
- `status === "success" && empty` → 空状态 (EmptyState + 可选的 empty-actions 插槽)
- 其他 → 渲染默认插槽内容

## CSS 设计 Token
所有自定义样式使用 `workspace-` 前缀: `workspace-panel-icon`, `workspace-empty-state`, `workspace-page-stack`, `workspace-danger-panel`, `workspace-metric-grid`, `workspace-mobile-card`, `workspace-sidebar-control`, `workspace-auth-card` 等

<important if="you are adding a new slideover editor component">
## 新增 Slideover 编辑器
1. 复制上方 Slideover Editor Pattern
2. 定义 props: `open`, `entity?`
3. 定义 emits: `"update:open"`, `"saved"`, 可选 `"request-delete"`
4. 使用 `useMutation($orpc.entity.create.mutationOptions())` 进行创建和更新
5. 用 `watch([open, entity])` 在打开/关闭时重置表单状态
6. 模板: `USlideover` side="right"，表单放在 `#body` 插槽中，编辑模式包含危险操作区
7. 在父页面中挂载: `<XSlideoverEditor :open="editorOpen" :entity="editingEntity" @update:open="..." />`
</important>

<important if="you are adding a new data list page">
## 新增数据列表页
1. 使用 `DataSurface` 作为区块包装器（不用已废弃的 `PageCard`）
2. 内部用 `QueryGuard` 包裹加载/错误/空/数据四种状态
3. 表格: `UTable` + 自定义单元格模板 + `RowActions`
4. 移动端适配: 桌面表格 (`hidden md:block`) + 移动端卡片 (`md:hidden`)
5. 上方加 `FilterBar`，下方加 `ListPagination`
6. 状态标签用 `label*()`/`color*()` 格式化函数 (来自 `~/utils/format`)
</important>
