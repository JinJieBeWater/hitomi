type EnvValue = null | number | string | undefined;
type EnvLike = Record<string, EnvValue>;

const defaultListenHost = "localhost";
const defaultPublicHost = "localhost";
const defaultPort = 3001;

function readValue(env: EnvLike, key: string): string | undefined {
  const value = env[key];

  if (value === null || value === undefined) {
    return undefined;
  }

  const text = String(value).trim();

  return text || undefined;
}

function normalizeListenHost(host?: string): string {
  if (!host) {
    return defaultListenHost;
  }

  return host;
}

function normalizePublicHost(host?: string): string {
  if (!host || host === "0.0.0.0" || host === "::") {
    return defaultPublicHost;
  }

  return host;
}

function normalizeOrigin(origin: string): string {
  return origin.endsWith("/") ? origin.slice(0, -1) : origin;
}

function parsePort(value: string | undefined): number {
  const port = Number.parseInt(value ?? "", 10);

  if (!Number.isInteger(port) || port <= 0) {
    return defaultPort;
  }

  return port;
}

export function resolveAppPort(env: EnvLike = process.env): number {
  const value = readValue(env, "PORT") ?? readValue(env, "APP_PORT") ?? readValue(env, "NUXT_PORT");

  return parsePort(value);
}

export function resolveAppListenHost(env: EnvLike = process.env): string {
  return normalizeListenHost(readValue(env, "HOST") ?? readValue(env, "APP_HOST"));
}

export function resolveAppHost(env: EnvLike = process.env): string {
  return normalizePublicHost(readValue(env, "HOST") ?? readValue(env, "APP_HOST"));
}

export function resolveAppOrigin(env: EnvLike = process.env): string {
  const explicit =
    readValue(env, "APP_BASE_URL") ??
    readValue(env, "BETTER_AUTH_URL") ??
    readValue(env, "CORS_ORIGIN") ??
    readValue(env, "NUXT_PUBLIC_SERVER_URL");

  if (explicit) {
    return normalizeOrigin(explicit);
  }

  return `http://${resolveAppHost(env)}:${resolveAppPort(env)}`;
}
