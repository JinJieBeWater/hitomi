import { db, faceProfile } from "@hitomi/db";
import { eq } from "drizzle-orm";
import { defineEventHandler } from "h3";

import {
  authenticateDevice,
  deviceFailure,
  deviceInternalError,
  deviceSuccess,
  enrollmentReportBodySchema,
  getFaceProfile,
  parseDeviceBody,
} from "../../../utils/device";

export default defineEventHandler(async (event) => {
  try {
    const parsed = await parseDeviceBody(event, enrollmentReportBodySchema);

    if (!parsed.ok) {
      return parsed.response;
    }

    const auth = await authenticateDevice(parsed.data.deviceCode, parsed.data.apiKey);

    if (!auth.ok) {
      return auth.response;
    }

    const task = await getFaceProfile(parsed.data.taskId);

    if (!task) {
      return deviceFailure("ENROLLMENT_TASK_NOT_FOUND", "enrollment task not found");
    }

    if (task.employeeId !== parsed.data.employeeId) {
      return deviceFailure("ENROLLMENT_TASK_MISMATCH", "enrollment task and employee do not match");
    }

    if (task.deviceId !== auth.device.id) {
      return deviceFailure("ENROLLMENT_TASK_MISMATCH", "enrollment task does not belong to current device");
    }

    if (task.status === "cancelled") {
      return deviceFailure("TASK_CANCELLED", "enrollment task was cancelled");
    }

    if (task.status !== "pending") {
      return deviceSuccess({
        taskId: task.id,
        employeeId: task.employeeId,
        status: task.status,
        applied: false,
      });
    }

    const nextStatus = parsed.data.result === "success" ? "success" : "failed";

    await db
      .update(faceProfile)
      .set({
        status: nextStatus,
      })
      .where(eq(faceProfile.id, task.id));

    return deviceSuccess({
      taskId: task.id,
      employeeId: task.employeeId,
      status: nextStatus,
      applied: true,
    });
  } catch (error) {
    return deviceInternalError(event, error);
  }
});
