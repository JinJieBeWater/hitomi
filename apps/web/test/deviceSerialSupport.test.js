import { expect, test } from "bun:test";

import { resolveDeviceSerialSupport } from "../app/utils/deviceSerialSupport.ts";

test("reports insecure context for Chromium without Web Serial API", () => {
  expect(
    resolveDeviceSerialSupport({
      hasSerialApi: false,
      isSecureContext: false,
      isChromiumBrowser: true,
    }),
  ).toEqual({
    available: false,
    reason: "insecure_context",
    isSecureContext: false,
    isChromiumBrowser: true,
  });
});

test("reports unsupported browser when non-Chromium browser has no Web Serial API", () => {
  expect(
    resolveDeviceSerialSupport({
      hasSerialApi: false,
      isSecureContext: true,
      isChromiumBrowser: false,
    }),
  ).toEqual({
    available: false,
    reason: "unsupported_browser",
    isSecureContext: true,
    isChromiumBrowser: false,
  });
});

test("reports available when Web Serial API exists", () => {
  expect(
    resolveDeviceSerialSupport({
      hasSerialApi: true,
      isSecureContext: true,
      isChromiumBrowser: true,
    }),
  ).toEqual({
    available: true,
    reason: "available",
    isSecureContext: true,
    isChromiumBrowser: true,
  });
});
