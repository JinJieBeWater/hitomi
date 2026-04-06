import { defineEventHandler } from "h3";

import {
  authenticateDevice,
  deviceInternalError,
  deviceSuccess,
  getSyncPayload,
  parseDeviceBody,
  syncBodySchema,
} from "../../utils/device";

export default defineEventHandler(async (event) => {
  try {
    const parsed = await parseDeviceBody(event, syncBodySchema);

    if (!parsed.ok) {
      return parsed.response;
    }

    const auth = await authenticateDevice(parsed.data.deviceCode, parsed.data.apiKey);

    if (!auth.ok) {
      return auth.response;
    }

    const payload = await getSyncPayload(auth.device.id);

    return deviceSuccess({
      serverTime: Date.now(),
      timezone: "Asia/Shanghai",
      device: {
        id: auth.device.id,
        name: auth.device.name,
        status: auth.device.status,
      },
      attendanceConfig: payload.attendanceConfig,
      employees: payload.employees,
      enrollmentTasks: payload.enrollmentTasks,
    });
  } catch (error) {
    return deviceInternalError(event, error);
  }
});
