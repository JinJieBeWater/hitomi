import { execFileSync } from "node:child_process";
import { mkdirSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";

import { diagrams, prepareDiagram } from "./render-thesis-figures.mjs";

const repoRoot = resolve(".");
const outSvgDir = join(repoRoot, "docs", "figures-fireworks-style6");
const outSrcDir = join(repoRoot, "docs", "figures-fireworks-style6-src");

mkdirSync(outSvgDir, { recursive: true });
mkdirSync(outSrcDir, { recursive: true });

for (const diagram of diagrams) {
  const prepared = prepareDiagram(diagram);
  const svg = renderStyle6Svg(prepared);
  const svgPath = join(outSvgDir, `${prepared.slug}.svg`);
  const pngPath = join(outSvgDir, `${prepared.slug}.png`);
  const htmlPath = join(outSrcDir, `${prepared.slug}.html`);

  writeFileSync(svgPath, svg, "utf8");
  writeFileSync(htmlPath, renderHtml(prepared), "utf8");
  execFileSync("rsvg-convert", ["-w", "1920", svgPath, "-o", pngPath], { stdio: "inherit" });
}

console.log(`Rendered ${diagrams.length} style-6 fireworks diagrams.`);
console.log(`SVG/PNG files: ${outSvgDir}`);
console.log(`HTML wrappers: ${outSrcDir}`);

function renderStyle6Svg(diagram) {
  const parts = [];
  parts.push(
    `<?xml version="1.0" encoding="UTF-8"?>`,
    `<svg xmlns="http://www.w3.org/2000/svg" width="1654" height="${diagram.height}" viewBox="0 0 1654 ${diagram.height}" role="img" aria-label="${escapeXml(diagram.title)}">`,
    `<style>
      text { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", "Helvetica Neue", Arial, sans-serif; }
      .title { font-size: 20px; font-weight: 700; fill: #1a1a1a; }
      .label { font-size: 16px; font-weight: 600; fill: #1a1a1a; }
      .detail { font-size: 14px; fill: #1a1a1a; }
      .arrow-label { font-size: 13px; fill: #5a5a5a; }
      .panel-label { font-size: 14px; font-weight: 600; fill: #6a6a6a; }
      .small { font-size: 12px; fill: #6a6a6a; }
    </style>`,
    `<defs>`,
    marker("arrow-claude", "#5a5a5a"),
    shadow(),
    `</defs>`,
    `<rect width="1654" height="${diagram.height}" fill="#f8f6f3"/>`,
    `<rect x="24" y="24" width="1606" height="${diagram.height - 48}" rx="12" ry="12" fill="#f8f6f3" stroke="#d8d0c7" stroke-width="1.5"/>`,
    centeredText(827, 50, diagram.title, "title"),
  );

  for (const shape of diagram.shapes) {
    if (shape.type === "border" || shape.type === "title") continue;
    if (shape.type === "panel") {
      parts.push(renderPanel(shape));
      continue;
    }
    if (shape.type === "tableEntity") {
      parts.push(renderTable(shape));
      continue;
    }
    if (shape.type === "node") {
      parts.push(renderNode(shape));
      continue;
    }
    if (shape.type === "diamond") {
      parts.push(renderDiamond(shape));
      continue;
    }
    if (shape.type === "cylinder") {
      parts.push(renderCylinder(shape));
      continue;
    }
    if (shape.type === "text") {
      parts.push(renderFreeText(shape));
    }
  }

  for (const edge of diagram.shapes.filter((shape) => shape.type === "edge")) {
    parts.push(renderEdge(edge, diagram));
  }

  parts.push(`</svg>`);
  return parts.join("\n");
}

function renderPanel(shape) {
  return [
    `<rect x="${shape.x}" y="${shape.y}" width="${shape.w}" height="${shape.h}" rx="12" ry="12" fill="none" stroke="#cfc5b7" stroke-width="1.6"/>`,
    `<line x1="${shape.x + 14}" y1="${shape.y + 38}" x2="${shape.x + shape.w - 14}" y2="${shape.y + 38}" stroke="#ddd4c8" stroke-width="1"/>`,
    textStart(shape.x + 18, shape.y + 24, shape.label, "panel-label"),
  ].join("\n");
}

function renderNode(shape) {
  const tone = palette(shape.semantic);
  const lines = shape.text;
  const lineHeight = shape.lineHeight ?? 20;
  const startY = shape.y + shape.h / 2 - ((lines.length - 1) * lineHeight) / 2;
  return [
    `<rect x="${shape.x}" y="${shape.y}" width="${shape.w}" height="${shape.h}" rx="12" ry="12" fill="${tone.fill}" stroke="#4a4a4a" stroke-width="2.5" filter="url(#shadow-soft)"/>`,
    ...lines.map((line, index) =>
      centeredText(
        shape.x + shape.w / 2,
        startY + index * lineHeight,
        line,
        index === 0 ? "label" : "detail",
      ),
    ),
  ].join("\n");
}

function renderDiamond(shape) {
  const cx = shape.x + shape.w / 2;
  const cy = shape.y + shape.h / 2;
  const points = `${cx},${shape.y} ${shape.x + shape.w},${cy} ${cx},${shape.y + shape.h} ${shape.x},${cy}`;
  const lines = shape.text;
  const lineHeight = shape.lineHeight ?? 18;
  const startY = cy - ((lines.length - 1) * lineHeight) / 2;
  return [
    `<polygon points="${points}" fill="#f4e4c1" stroke="#4a4a4a" stroke-width="2.5" filter="url(#shadow-soft)"/>`,
    ...lines.map((line, index) =>
      centeredText(cx, startY + index * lineHeight, line, index === 0 ? "label" : "detail"),
    ),
  ].join("\n");
}

function renderCylinder(shape) {
  const tone = palette(shape.semantic);
  const cx = shape.x + shape.w / 2;
  const rx = shape.w / 2;
  const ry = 14;
  const top = shape.y + ry;
  const bottom = shape.y + shape.h - ry;
  const lines = shape.text;
  const lineHeight = shape.lineHeight ?? 20;
  const startY = shape.y + shape.h / 2 - ((lines.length - 1) * lineHeight) / 2;
  return [
    `<ellipse cx="${cx}" cy="${top}" rx="${rx}" ry="${ry}" fill="${tone.fill}" stroke="#4a4a4a" stroke-width="2.5"/>`,
    `<rect x="${shape.x}" y="${top}" width="${shape.w}" height="${bottom - top}" fill="${tone.fill}" stroke="none"/>`,
    `<line x1="${shape.x}" y1="${top}" x2="${shape.x}" y2="${bottom}" stroke="#4a4a4a" stroke-width="2.5"/>`,
    `<line x1="${shape.x + shape.w}" y1="${top}" x2="${shape.x + shape.w}" y2="${bottom}" stroke="#4a4a4a" stroke-width="2.5"/>`,
    `<ellipse cx="${cx}" cy="${bottom}" rx="${rx}" ry="${ry}" fill="${tone.fill}" stroke="#4a4a4a" stroke-width="2.5"/>`,
    ...lines.map((line, index) =>
      centeredText(cx, startY + index * lineHeight, line, index === 0 ? "label" : "detail"),
    ),
  ].join("\n");
}

function renderTable(shape) {
  const tone = palette(shape.semantic);
  const headerH = 34;
  const colHeadH = 28;
  const rowH = Math.round((shape.bodyFontSize ?? 13) * 1.55) + 8;
  const widths =
    shape.columns.length === 3
      ? [
          Math.floor(shape.w * 0.26),
          Math.floor(shape.w * 0.52),
          shape.w - Math.floor(shape.w * 0.26) - Math.floor(shape.w * 0.52),
        ]
      : [Math.floor(shape.w * 0.54), shape.w - Math.floor(shape.w * 0.54)];
  const x2 = shape.x + widths[0];
  const x3 = shape.columns.length === 3 ? x2 + widths[1] : null;
  const items = [
    `<rect x="${shape.x}" y="${shape.y}" width="${shape.w}" height="${shape.h}" rx="12" ry="12" fill="#ffffff" stroke="#4a4a4a" stroke-width="2.2"/>`,
    `<rect x="${shape.x}" y="${shape.y}" width="${shape.w}" height="${headerH}" rx="12" ry="12" fill="${tone.fill}" stroke="#4a4a4a" stroke-width="2.2"/>`,
    centeredText(shape.x + shape.w / 2, shape.y + 20, shape.title, "label"),
    `<rect x="${shape.x}" y="${shape.y + headerH}" width="${shape.w}" height="${colHeadH}" fill="#f7f3ed" stroke="none"/>`,
    `<line x1="${shape.x}" y1="${shape.y + headerH}" x2="${shape.x + shape.w}" y2="${shape.y + headerH}" stroke="#4a4a4a" stroke-width="1.4"/>`,
    `<line x1="${shape.x}" y1="${shape.y + headerH + colHeadH}" x2="${shape.x + shape.w}" y2="${shape.y + headerH + colHeadH}" stroke="#4a4a4a" stroke-width="1.2"/>`,
    `<line x1="${x2}" y1="${shape.y + headerH}" x2="${x2}" y2="${shape.y + shape.h}" stroke="#4a4a4a" stroke-width="1"/>`,
  ];
  if (x3 !== null)
    items.push(
      `<line x1="${x3}" y1="${shape.y + headerH}" x2="${x3}" y2="${shape.y + shape.h}" stroke="#4a4a4a" stroke-width="1"/>`,
    );
  shape.columns.forEach((col, i) => {
    const ox = i === 0 ? shape.x : i === 1 ? x2 : x3;
    items.push(
      centeredText(ox + widths[i] / 2, shape.y + headerH + colHeadH / 2 + 1, col, "small"),
    );
  });
  shape.rows.forEach((row, r) => {
    const y = shape.y + headerH + colHeadH + r * rowH;
    items.push(
      `<line x1="${shape.x}" y1="${y}" x2="${shape.x + shape.w}" y2="${y}" stroke="#b5a99a" stroke-width="0.8"/>`,
    );
    row.forEach((cell, c) => {
      const ox = c === 0 ? shape.x : c === 1 ? x2 : x3;
      items.push(textStart(ox + 8, y + rowH / 2 + 1, cell, "small"));
    });
  });
  return items.join("\n");
}

function renderFreeText(shape) {
  return shape.text
    .map((line, index) =>
      centeredText(
        shape.x + shape.w / 2,
        shape.y + 10 + index * (shape.fontSize ?? 14),
        line,
        "arrow-label",
      ),
    )
    .join("\n");
}

function renderEdge(edge, diagram) {
  const strokeWidth = edge.semantic === "optional" ? 1.5 : 2;
  const dash =
    edge.semantic === "optional"
      ? ` stroke-dasharray="3,2"`
      : edge.semantic === "context"
        ? ``
        : ``;
  const from = connector(diagram, edge.from, edge.fromAnchor ?? "right");
  const to = connector(diagram, edge.to, edge.toAnchor ?? "left");
  const points = orthogonal([from, ...(edge.points ?? []), to], edge.fromAnchor, edge.toAnchor);
  return `<polyline points="${points.map((p) => `${p.x},${p.y}`).join(" ")}" fill="none" stroke="#5a5a5a" stroke-width="${strokeWidth}"${dash} marker-end="url(#arrow-claude)"/>`;
}

function palette(semantic) {
  switch (semantic) {
    case "start":
    case "secondary":
      return { fill: "#a8c5e6" };
    case "ai":
    case "end":
      return { fill: "#9dd4c7" };
    case "tertiary":
    case "decision":
      return { fill: "#f4e4c1" };
    case "inactive":
    case "primary":
    default:
      return { fill: "#e8e6e3" };
  }
}

function renderHtml(diagram) {
  return `<!doctype html>
<html lang="zh-CN">
<head><meta charset="UTF-8"/><meta name="viewport" content="width=device-width, initial-scale=1.0"/><title>${escapeXml(diagram.title)} style6</title>
<style>body{margin:0;padding:24px;background:#fff}.figure-wrap{width:fit-content;margin:0 auto}img{display:block;max-width:min(100vw - 48px,1500px);height:auto}</style>
</head>
<body><div class="figure-wrap"><img src="../figures-fireworks-style6/${diagram.slug}.svg" alt="${escapeXml(diagram.title)} style6"/></div></body></html>`;
}

function marker(id, fill) {
  return `<marker id="${id}" markerWidth="8" markerHeight="8" refX="7" refY="4" orient="auto"><polygon points="0 0, 8 4, 0 8" fill="${fill}"/></marker>`;
}

function shadow() {
  return `<filter id="shadow-soft"><feDropShadow dx="0" dy="2" stdDeviation="6" flood-color="#00000008"/></filter>`;
}

function centeredText(x, y, text, className) {
  return `<text x="${x}" y="${y}" text-anchor="middle" dominant-baseline="middle" class="${className}">${escapeXml(text)}</text>`;
}

function textStart(x, y, text, className) {
  return `<text x="${x}" y="${y}" text-anchor="start" dominant-baseline="middle" class="${className}">${escapeXml(text)}</text>`;
}

function connector(diagram, id, mode) {
  const shape = diagram.shapes.find((item) => item.id === id);
  if (!shape) throw new Error(`Unknown shape id: ${id}`);
  switch (mode) {
    case "left":
      return { x: shape.x, y: shape.y + shape.h / 2 };
    case "right":
      return { x: shape.x + shape.w, y: shape.y + shape.h / 2 };
    case "top":
      return { x: shape.x + shape.w / 2, y: shape.y };
    case "bottom":
      return { x: shape.x + shape.w / 2, y: shape.y + shape.h };
    default:
      return { x: shape.x + shape.w, y: shape.y + shape.h / 2 };
  }
}

function orthogonal(points, fromAnchor, toAnchor) {
  const result = [points[0]];
  for (let i = 1; i < points.length; i += 1) {
    const prev = result[result.length - 1];
    const next = points[i];
    if (prev.x === next.x || prev.y === next.y) {
      result.push(next);
      continue;
    }
    const horizontalFirst =
      fromAnchor === "left" ||
      fromAnchor === "right" ||
      toAnchor === "left" ||
      toAnchor === "right";
    const corner = horizontalFirst ? { x: next.x, y: prev.y } : { x: prev.x, y: next.y };
    result.push(corner, next);
  }
  return dedupe(result);
}

function dedupe(points) {
  const out = [];
  for (const p of points) {
    const last = out.at(-1);
    if (last && last.x === p.x && last.y === p.y) continue;
    out.push(p);
    while (out.length >= 3) {
      const a = out[out.length - 3];
      const b = out[out.length - 2];
      const c = out[out.length - 1];
      const col = (a.x === b.x && b.x === c.x) || (a.y === b.y && b.y === c.y);
      if (!col) break;
      out.splice(out.length - 2, 1);
    }
  }
  return out;
}

function escapeXml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&apos;");
}
