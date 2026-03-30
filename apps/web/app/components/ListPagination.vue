<script setup lang="ts">
const props = withDefaults(
  defineProps<{
    page: number;
    pageSize: number;
    total: number;
    disabled?: boolean;
  }>(),
  {
    disabled: false,
  },
);

const emit = defineEmits<{
  "update:page": [value: number];
}>();

const pageCount = computed(() => Math.max(1, Math.ceil(props.total / props.pageSize)));
const start = computed(() => (props.total === 0 ? 0 : (props.page - 1) * props.pageSize + 1));
const end = computed(() => (props.total === 0 ? 0 : Math.min(props.page * props.pageSize, props.total)));
</script>

<template>
  <div class="flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between">
    <div class="text-sm text-toned">
      显示 {{ start }} - {{ end }}，共 {{ props.total }} 条
    </div>

    <UPagination
      v-if="pageCount > 1"
      :page="props.page"
      :items-per-page="props.pageSize"
      :total="props.total"
      :disabled="props.disabled"
      :sibling-count="1"
      show-edges
      @update:page="emit('update:page', $event)"
    />
  </div>
</template>
