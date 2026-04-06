import { db } from "@hitomi/db";
import * as schema from "@hitomi/db/schema/auth";
import { env } from "@hitomi/env/server";
import { betterAuth } from "better-auth";
import { drizzleAdapter } from "better-auth/adapters/drizzle";

function normalizeOrigin(value: string) {
  return new URL(value).origin;
}

function resolveRequestHostOrigin(request?: Request) {
  const host = request?.headers.get("x-forwarded-host") ?? request?.headers.get("host");

  if (!host) {
    return null;
  }

  const protocol =
    request?.headers.get("x-forwarded-proto") ??
    new URL(env.BETTER_AUTH_URL).protocol.replace(":", "");

  return `${protocol}://${host}`;
}

const configuredTrustedOrigins = [
  normalizeOrigin(env.CORS_ORIGIN),
  normalizeOrigin(env.BETTER_AUTH_URL),
];

export const auth = betterAuth({
  database: drizzleAdapter(db, {
    provider: "sqlite",

    schema: schema,
  }),
  trustedOrigins: async (request) => {
    const requestHostOrigin = resolveRequestHostOrigin(request);

    return [
      ...new Set([
        ...configuredTrustedOrigins,
        ...(requestHostOrigin ? [normalizeOrigin(requestHostOrigin)] : []),
      ]),
    ];
  },
  emailAndPassword: {
    enabled: true,
  },
  secret: env.BETTER_AUTH_SECRET,
  baseURL: env.BETTER_AUTH_URL,
  plugins: [],
});
