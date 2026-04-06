import { defineEventHandler } from "h3";

import {
  activationStatusBodySchema,
  authenticateBootstrapDevice,
  buildBootstrapActivationResponse,
  deviceFailure,
  deviceInternalError,
  deviceSuccess,
  parseDeviceBody,
} from "../../../utils/device";

export default defineEventHandler(async (event) => {
  try {
    const parsed = await parseDeviceBody(event, activationStatusBodySchema);

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

    const activation = auth.activation;
    if (activation.id !== parsed.data.registrationId) {
      return deviceFailure("INVALID_REQUEST", "registrationId is invalid");
    }

    return deviceSuccess(await buildBootstrapActivationResponse(activation));
  } catch (error) {
    return deviceInternalError(event, error);
  }
});
