import { existsSync } from "node:fs";

import dotenv from "dotenv";
import { createEnv } from "@t3-oss/env-core";
import { z } from "zod";
import { resolveAppHost, resolveAppOrigin, resolveAppPort } from "./app";
import { resolveWebEnvPath } from "./paths";

export const serverEnvPath = resolveWebEnvPath();

if (existsSync(serverEnvPath)) {
  dotenv.config({
    path: serverEnvPath,
  });
}

const rawEnv = createEnv({
  server: {
    DATABASE_URL: z.string().min(1),
    BETTER_AUTH_SECRET: z.string().min(32),
    APP_HOST: z.string().min(1).optional(),
    APP_PORT: z.coerce.number().int().positive().optional(),
    APP_BASE_URL: z.url().optional(),
    BETTER_AUTH_URL: z.url().optional(),
    CORS_ORIGIN: z.url().optional(),
    NODE_ENV: z.enum(["development", "production", "test"]).default("development"),
  },
  runtimeEnv: process.env,
  emptyStringAsUndefined: true,
});

const appOrigin = resolveAppOrigin(process.env);

export const env = {
  ...rawEnv,
  APP_HOST: rawEnv.APP_HOST ?? resolveAppHost(process.env),
  APP_PORT: rawEnv.APP_PORT ?? resolveAppPort(process.env),
  APP_BASE_URL: rawEnv.APP_BASE_URL ?? appOrigin,
  BETTER_AUTH_URL: rawEnv.BETTER_AUTH_URL ?? appOrigin,
  CORS_ORIGIN: rawEnv.CORS_ORIGIN ?? appOrigin,
} as const;
