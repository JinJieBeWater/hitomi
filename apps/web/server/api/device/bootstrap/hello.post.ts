import { defineEventHandler } from "h3";

import {
  authenticateBootstrapDevice,
  buildBootstrapActivationResponse,
  bootstrapHelloBodySchema,
  deviceInternalError,
  deviceSuccess,
  parseDeviceBody,
} from "../../../utils/device";

export default defineEventHandler(async (event) => {
  try {
    const parsed = await parseDeviceBody(event, bootstrapHelloBodySchema);

    if (!parsed.ok) {
      return parsed.response;
    }

    const auth = await authenticateBootstrapDevice(
      parsed.data.deviceSerial,
      parsed.data.bootstrapSecret,
    );
    if (!auth.ok) {
      return auth.response;
    }

    return deviceSuccess(await buildBootstrapActivationResponse(auth.activation));
  } catch (error) {
    return deviceInternalError(event, error);
  }
});
