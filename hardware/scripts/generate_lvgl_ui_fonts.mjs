#!/usr/bin/env node

import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import path from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const scriptPath = fileURLToPath(import.meta.url);
const scriptDir = path.dirname(scriptPath);
const hardwareDir = path.resolve(scriptDir, "..");
const fontDir = path.join(hardwareDir, "src/fonts");
const symbolsFile = path.join(fontDir, "hitomi_ui_font_symbols.txt");

const uiStringFiles = [
  "include/app/runtime_face_status.hpp",
  "src/app/runtime_enrollment_ops.cpp",
  "src/app/runtime_face_engine_ops.cpp",
  "src/app/runtime_network_ops.cpp",
  "src/infra/display/lvgl_status_display.cpp",
  "src/infra/face/esp_who_enrollment_service.cpp",
  "src/ui/status_screen_presenter.cpp",
];

const fontTargets = [
  {
    size: 12,
    fontName: "hitomi_ui_zh_12",
    outputPath: path.join(fontDir, "hitomi_ui_zh_12.c"),
  },
  {
    size: 14,
    fontName: "hitomi_ui_zh_14",
    outputPath: path.join(fontDir, "hitomi_ui_zh_14.c"),
  },
];

function cppStringLiterals(source) {
  return source.match(/"(?:\\.|[^"\\])*"/g) ?? [];
}

function extractUiSymbols() {
  const symbols = new Set();

  for (const relativePath of uiStringFiles) {
    const absolutePath = path.join(hardwareDir, relativePath);
    const source = readFileSync(absolutePath, "utf8");
    for (const literal of cppStringLiterals(source)) {
      for (const ch of literal.slice(1, -1)) {
        if (/\s/u.test(ch)) {
          continue;
        }
        if (ch.charCodeAt(0) <= 0x7f) {
          continue;
        }
        symbols.add(ch);
      }
    }
  }

  return [...symbols].sort().join("");
}

function resolveFontPath() {
  const candidates = [
    process.env.HITOMI_UI_FONT_PATH,
    "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
    "/System/Library/Fonts/Supplemental/NISC18030.ttf",
    "/System/Library/Fonts/Hiragino Sans GB.ttc",
  ].filter(Boolean);

  for (const candidate of candidates) {
    if (existsSync(candidate)) {
      return candidate;
    }
  }

  throw new Error(
    "No usable CJK font found. Set HITOMI_UI_FONT_PATH to a .ttf/.otf/.ttc font before generating.",
  );
}

function runLvFontConv(fontPath, symbols, target) {
  const args = [
    "--yes",
    "lv_font_conv",
    "--size",
    String(target.size),
    "--bpp",
    "4",
    "--format",
    "lvgl",
    "--lv-include",
    "lvgl.h",
    "--font",
    fontPath,
    "-r",
    "0x20-0x7F",
    "--symbols",
    symbols,
    "--force-fast-kern-format",
    "--lv-font-name",
    target.fontName,
    "-o",
    target.outputPath,
  ];

  const result = spawnSync("npx", args, {
    cwd: hardwareDir,
    encoding: "utf8",
    env: {
      ...process.env,
      npm_config_cache: process.env.npm_config_cache || "/tmp/codex-npm-cache",
    },
  });

  if (result.status !== 0) {
    throw new Error(result.stderr || result.stdout || "lv_font_conv failed");
  }
}

function ensureCoverageMatchesCurrentStrings(currentSymbols) {
  if (!existsSync(symbolsFile)) {
    throw new Error(`Missing ${path.relative(hardwareDir, symbolsFile)}. Run generate first.`);
  }

  const savedSymbols = readFileSync(symbolsFile, "utf8").trim();
  if (savedSymbols !== currentSymbols) {
    throw new Error(
      "UI font symbols are out of date. Re-run hardware/scripts/generate_lvgl_ui_fonts.mjs generate.",
    );
  }
}

function generate() {
  mkdirSync(fontDir, { recursive: true });
  const symbols = extractUiSymbols();
  writeFileSync(symbolsFile, `${symbols}\n`, "utf8");

  const fontPath = resolveFontPath();
  for (const target of fontTargets) {
    runLvFontConv(fontPath, symbols, target);
  }

  console.log(`Generated fonts from ${fontPath}`);
  console.log(`Symbols: ${symbols.length}`);
}

function check() {
  const symbols = extractUiSymbols();
  ensureCoverageMatchesCurrentStrings(symbols);
  console.log(`Font symbols up to date (${symbols.length} symbols)`);
}

const mode = process.argv[2] ?? "generate";
if (mode === "generate") {
  generate();
} else if (mode === "check") {
  check();
} else {
  throw new Error(`Unknown mode: ${mode}`);
}
