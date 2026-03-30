import { dirname, resolve } from "node:path";
import { pathToFileURL } from "node:url";

import { resolveWebEnvPath } from "@hitomi/env/paths";
import dotenv from "dotenv";
import { defineConfig } from "drizzle-kit";

const envPath = resolveWebEnvPath();

dotenv.config({
  path: envPath,
});

function normalizeDatabaseUrl(value: string): string {
  if (!value.startsWith("file:")) {
    return value;
  }

  const filePath = value.slice("file:".length);

  if (!filePath || filePath.startsWith("//")) {
    return value;
  }

  if (filePath.startsWith("/")) {
    return pathToFileURL(filePath).href;
  }

  return pathToFileURL(resolve(dirname(envPath), filePath)).href;
}

export default defineConfig({
  schema: "./src/schema",
  out: "./src/migrations",
  dialect: "turso",
  dbCredentials: {
    url: normalizeDatabaseUrl(process.env.DATABASE_URL || ""),
  },
});
