import { db } from "@hitomi/db";

import { protectedProcedure } from "../index";
import { getDashboardSummary } from "../lib/utils";

export const dashboardRouter = {
  summary: protectedProcedure.handler(async () => {
    return getDashboardSummary(db);
  }),
};
