import { renderMermaidSVG } from "beautiful-mermaid";
import { mkdirSync, writeFileSync } from "node:fs";

const OUT = "docs/defense/diagrams";
mkdirSync(OUT, { recursive: true });

const theme = {
  bg: "#FFFFFF",
  fg: "#1e293b",
  accent: "#2563eb",
  muted: "#64748b",
  surface: "#f1f5f9",
  border: "#cbd5e1",
  font: "Inter, system-ui, sans-serif",
};

// ── P2: 系统架构图 — 三层 + 三种协议，极简 ──
const arch = `
flowchart LR
  WEB["🖥️ 管理后台<br/>Nuxt 3"]
  SVR["⚙️ 后端服务<br/>oRPC + SQLite"]
  DEV["📱 ESP32-S3 终端<br/>摄像头 · 触摸屏 · Wi-Fi"]

  WEB <-->|"oRPC 类型安全 RPC"| SVR
  SVR <-->|"HTTP REST 设备同步"| DEV
  WEB -.->|"Web Serial 首配"| DEV
`;

// ── P3: 考勤核心流程 — 只保留主链路 ──
const flow = `
flowchart LR
  A["📷 人脸采集<br/>+ ESP-WHO 识别"] --> B["👤 匹配员工身份"]
  B --> C["⏰ 考勤窗口判定<br/>上班 / 下班"]
  C --> D["📤 上传服务端<br/>离线时缓存 SD 后补传"]
  D --> E["✅ 二次校验入库"]
`;

// ── P6: 技术栈分层 — 保持 ──
const tech = `
flowchart LR
  subgraph L1["🖥️ Web 管理端"]
    W1["Nuxt 3"]
    W2["oRPC"]
    W3["TailwindCSS"]
  end
  subgraph L2["⚙️ 后端服务"]
    B1["Drizzle ORM"]
    B2["SQLite"]
    B3["Better Auth"]
  end
  subgraph L3["📱 嵌入式终端"]
    D1["ESP32-S3"]
    D2["ESP-WHO"]
    D3["LVGL · Wi-Fi · SD"]
  end
  subgraph L4["🔧 工程工具"]
    T1["Turborepo"]
    T2["PlatformIO"]
  end
  L1 --> L2 --> L3
  L4 -.-> L1
  L4 -.-> L3
`;

const diagrams = [
  { name: "01-system-architecture", code: arch },
  { name: "02-attendance-flow", code: flow },
  { name: "03-tech-stack", code: tech },
];

for (const { name, code } of diagrams) {
  const svg = renderMermaidSVG(code.trim(), { ...theme, padding: 24 });
  writeFileSync(`${OUT}/${name}.svg`, svg);
  console.log(`✅ ${name}.svg`);
}

console.log(`\nDone — ${diagrams.length} SVGs in ${OUT}/`);
