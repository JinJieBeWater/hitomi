export type PageInfo = {
  page: number;
  pageSize: number;
  total: number;
  totalPages: number;
};

export type PaginatedResult<T> = {
  items: T[];
  pageInfo: PageInfo;
};

export function createEmptyPageInfo(pageSize: number): PageInfo {
  return {
    page: 1,
    pageSize,
    total: 0,
    totalPages: 1,
  };
}

export async function fetchAllPages<T>(
  fetchPage: (page: number) => Promise<PaginatedResult<T>>,
): Promise<T[]> {
  const firstPage = await fetchPage(1);

  if (firstPage.pageInfo.totalPages <= 1) {
    return firstPage.items;
  }

  const remainingPages = await Promise.all(
    Array.from({ length: firstPage.pageInfo.totalPages - 1 }, (_, index) => fetchPage(index + 2)),
  );

  return firstPage.items.concat(...remainingPages.map((page) => page.items));
}
