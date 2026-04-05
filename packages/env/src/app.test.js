import { strict as assert } from "node:assert";
import test from "node:test";

import { resolveAppListenHost, resolveAppOrigin } from "./app.ts";

test("resolveAppListenHost preserves 0.0.0.0 for LAN dev server binding", () => {
  assert.equal(
    resolveAppListenHost({
      APP_HOST: "0.0.0.0",
    }),
    "0.0.0.0",
  );
});

test("resolveAppOrigin falls back to localhost when no explicit public origin is configured", () => {
  assert.equal(
    resolveAppOrigin({
      APP_HOST: "0.0.0.0",
      APP_PORT: "3001",
    }),
    "http://localhost:3001",
  );
});

test("resolveAppOrigin prefers explicit LAN public origin", () => {
  assert.equal(
    resolveAppOrigin({
      APP_HOST: "0.0.0.0",
      APP_PORT: "3001",
      APP_BASE_URL: "http://192.168.100.100:3001",
    }),
    "http://192.168.100.100:3001",
  );
});
