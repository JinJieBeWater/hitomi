import { existsSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const workspaceMarkers = ["package.json", "bun.lock", "turbo.json", "apps/web/package.json"];

function hasWorkspaceMarkers(dir: string) {
  return workspaceMarkers.every((marker) => existsSync(resolve(dir, marker)));
}

function findWorkspaceRoot(start: string) {
  let dir = resolve(start);

  while (true) {
    if (hasWorkspaceMarkers(dir)) {
      return dir;
    }

    const parent = dirname(dir);

    if (parent === dir) {
      return null;
    }

    dir = parent;
  }
}

function safeGetCwd() {
  try {
    return process.cwd();
  } catch {
    return null;
  }
}

export function resolveWorkspaceRoot() {
  const seeds = [
    safeGetCwd(),
    process.env.INIT_CWD,
    process.env.PWD,
    dirname(fileURLToPath(import.meta.url)),
  ].filter(Boolean) as string[];

  for (const seed of seeds) {
    const root = findWorkspaceRoot(seed);

    if (root) {
      return root;
    }
  }

  throw new Error("Unable to resolve workspace root.");
}

export function resolveWebEnvPath() {
  return resolve(resolveWorkspaceRoot(), "apps/web/.env");
}
