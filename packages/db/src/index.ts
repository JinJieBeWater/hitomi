import { dirname, resolve } from "node:path";
import { pathToFileURL } from "node:url";

import { env, serverEnvPath } from "@hitomi/env/server";
import { createClient } from "@libsql/client";
import { drizzle } from "drizzle-orm/libsql";

import * as schema from "./schema";

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

  const absolutePath = resolve(dirname(serverEnvPath), filePath);

  return pathToFileURL(absolutePath).href;
}

const client = createClient({
  url: normalizeDatabaseUrl(env.DATABASE_URL),
});

export const db = drizzle({ client, schema });
export { and, eq, gt } from "drizzle-orm";
export * from "./schema";
