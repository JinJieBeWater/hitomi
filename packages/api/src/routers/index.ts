import type { RouterClient } from "@orpc/server";

import { attendanceConfigRouter } from "./attendance-config";
import { attendanceRecordRouter } from "./attendance-record";
import { dashboardRouter } from "./dashboard";
import { deviceRouter } from "./device";
import { employeeRouter } from "./employee";
import { faceProfileRouter } from "./face-profile";
import { protectedProcedure, publicProcedure } from "../index";

export const appRouter = {
  dashboard: dashboardRouter,
  employee: employeeRouter,
  device: deviceRouter,
  attendanceConfig: attendanceConfigRouter,
  faceProfile: faceProfileRouter,
  attendanceRecord: attendanceRecordRouter,
  healthCheck: publicProcedure.handler(() => {
    return "OK";
  }),
  privateData: protectedProcedure.handler(({ context }) => {
    return {
      message: "This is private",
      user: context.session?.user,
    };
  }),
};
export type AppRouter = typeof appRouter;
export type AppRouterClient = RouterClient<typeof appRouter>;
