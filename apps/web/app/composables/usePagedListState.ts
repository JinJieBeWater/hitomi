import { computed, watch, type Ref, type WatchSource } from "vue";

import { createEmptyPageInfo, type PageInfo } from "~/utils/pagination";

type UsePagedListStateInput = {
  page: Ref<number>;
  pageSize: number;
  getPageInfo: () => PageInfo | undefined;
  resetOn?: WatchSource<unknown> | WatchSource<unknown>[];
};

export function usePagedListState(input: UsePagedListStateInput) {
  const { page } = input;
  const pageInfo = computed(() => input.getPageInfo() ?? createEmptyPageInfo(input.pageSize));
  const total = computed(() => pageInfo.value.total);

  if (input.resetOn) {
    watch(input.resetOn, resetPage);
  }

  watch(
    () => input.getPageInfo()?.totalPages,
    function keepPageWithinRange(totalPages) {
      if (totalPages && page.value > totalPages) {
        page.value = totalPages;
      }
    },
  );

  function resetPage(): void {
    page.value = 1;
  }

  return {
    page,
    pageInfo,
    total,
    resetPage,
  };
}
