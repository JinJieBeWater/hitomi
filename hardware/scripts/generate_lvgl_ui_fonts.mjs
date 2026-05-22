#!/usr/bin/env node

import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import os from "node:os";
import path from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const scriptPath = fileURLToPath(import.meta.url);
const scriptDir = path.dirname(scriptPath);
const hardwareDir = path.resolve(scriptDir, "..");
const fontDir = path.join(hardwareDir, "src/fonts");
const symbolsFile = path.join(fontDir, "hitomi_ui_font_symbols.txt");
const subsetDataFile = path.join(fontDir, "hitomi_ui_subset_ttf.c");
const subsetWorkDir = path.join(os.tmpdir(), "hitomi-ui-fonts");
const subsetTtfFile = path.join(subsetWorkDir, "hitomi_ui_subset.ttf");
const sourceRoots = ["include", "src"];
const sourceExtensions = new Set([".hpp", ".cpp", ".c"]);

function cppStringLiterals(source) {
  return source.match(/"(?:\\.|[^"\\])*"/g) ?? [];
}

function walkSourceFiles(directoryPath, files = []) {
  for (const entry of readdirSync(directoryPath, { withFileTypes: true })) {
    const absolutePath = path.join(directoryPath, entry.name);
    const relativePath = path.relative(hardwareDir, absolutePath);

    if (entry.isDirectory()) {
      if (relativePath === "src/fonts") {
        continue;
      }
      walkSourceFiles(absolutePath, files);
      continue;
    }

    if (!sourceExtensions.has(path.extname(entry.name))) {
      continue;
    }

    files.push(relativePath);
  }

  return files;
}

function collectUiSourceFiles() {
  // Scan all checked-in device sources so new static UI copy doesn't require
  // manually updating a file allowlist before regenerating fonts.
  return sourceRoots.flatMap((root) => walkSourceFiles(path.join(hardwareDir, root))).sort();
}

function extractUiSymbols() {
  const symbols = new Set();
  const sourceFiles = collectUiSourceFiles();

  for (const relativePath of sourceFiles) {
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
    "/System/Library/Fonts/Hiragino Sans GB.ttc",
    "/System/Library/Fonts/Supplemental/NISC18030.ttf",
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

function resolveAsciiSymbols() {
  let asciiSymbols = "";
  for (let code = 0x20; code <= 0x7e; code += 1) {
    asciiSymbols += String.fromCharCode(code);
  }
  return asciiSymbols;
}

function runFontSubset(fontPath, symbols) {
  mkdirSync(subsetWorkDir, { recursive: true });
  const textFile = path.join(subsetWorkDir, "hitomi_ui_subset_chars.txt");
  writeFileSync(textFile, `${resolveAsciiSymbols()}${symbols}\n`, "utf8");

  const args = [
    "-m",
    "fontTools.subset",
    fontPath,
    `--text-file=${textFile}`,
    `--output-file=${subsetTtfFile}`,
    "--no-hinting",
  ];

  const result = spawnSync("python3", args, {
    cwd: hardwareDir,
    encoding: "utf8",
  });

  if (result.status !== 0) {
    throw new Error(
      "fontTools subset failed. Install fonttools first, for example: python3 -m pip install fonttools",
    );
  }
}

function runXxd() {
  const args = ["-i", "-n", "hitomi_ui_subset_ttf", subsetTtfFile];

  const result = spawnSync("xxd", args, {
    cwd: hardwareDir,
    encoding: "utf8",
  });

  if (result.status !== 0) {
    throw new Error(result.stderr || result.stdout || "xxd failed");
  }

  const source = result.stdout
    .replace(/^unsigned char /m, "const unsigned char ")
    .replace(/^unsigned int /m, "const unsigned int ");
  writeFileSync(subsetDataFile, source, "utf8");
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
  runFontSubset(fontPath, symbols);
  runXxd();

  console.log(`Generated fonts from ${fontPath}`);
  console.log(`Symbols: ${symbols.length}`);
  console.log(`Embedded data: ${subsetDataFile}`);
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
