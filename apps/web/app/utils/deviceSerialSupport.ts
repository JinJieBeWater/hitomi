export type DeviceSerialSupportReason =
  | "available"
  | "insecure_context"
  | "unsupported_browser"
  | "unavailable";

export type DeviceSerialSupportSnapshot = {
  hasSerialApi: boolean;
  isSecureContext: boolean;
  isChromiumBrowser: boolean;
};

export type DeviceSerialSupportInfo = {
  available: boolean;
  reason: DeviceSerialSupportReason;
  isSecureContext: boolean;
  isChromiumBrowser: boolean;
};

export function resolveDeviceSerialSupport(
  snapshot: DeviceSerialSupportSnapshot,
): DeviceSerialSupportInfo {
  if (snapshot.hasSerialApi) {
    return {
      available: true,
      reason: "available",
      isSecureContext: snapshot.isSecureContext,
      isChromiumBrowser: snapshot.isChromiumBrowser,
    };
  }

  if (!snapshot.isSecureContext && snapshot.isChromiumBrowser) {
    return {
      available: false,
      reason: "insecure_context",
      isSecureContext: snapshot.isSecureContext,
      isChromiumBrowser: snapshot.isChromiumBrowser,
    };
  }

  if (!snapshot.isChromiumBrowser) {
    return {
      available: false,
      reason: "unsupported_browser",
      isSecureContext: snapshot.isSecureContext,
      isChromiumBrowser: snapshot.isChromiumBrowser,
    };
  }

  return {
    available: false,
    reason: "unavailable",
    isSecureContext: snapshot.isSecureContext,
    isChromiumBrowser: snapshot.isChromiumBrowser,
  };
}
