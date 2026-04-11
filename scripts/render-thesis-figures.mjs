import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const __dirname = dirname(fileURLToPath(import.meta.url));
const repoRoot = resolve(__dirname, "..");
const figuresDir = join(repoRoot, "docs", "figures");
const figureSrcDir = join(repoRoot, "docs", "figures-src");

const canvas = {
  width: 1654,
  background: "#F2EFE8",
};

const semanticStyles = {
  primary: {
    fill: "#E6E2DA",
    stroke: "#8C867F",
    text: "#2D2B28",
    drawio:
      "rounded=1;whiteSpace=wrap;arcSize=10;html=1;fillColor=#E6E2DA;strokeColor=#8C867F;strokeWidth=1.8;fontColor=#2D2B28;fontSize=18;",
  },
  secondary: {
    fill: "#EAF4FB",
    stroke: "#6FA8D6",
    text: "#2D2B28",
    drawio:
      "rounded=1;whiteSpace=wrap;arcSize=10;html=1;fillColor=#EAF4FB;strokeColor=#6FA8D6;strokeWidth=1.8;fontColor=#2D2B28;fontSize=18;",
  },
  tertiary: {
    fill: "#EEEAF9",
    stroke: "#9A90D6",
    text: "#2D2B28",
    drawio:
      "rounded=1;whiteSpace=wrap;arcSize=10;html=1;fillColor=#EEEAF9;strokeColor=#9A90D6;strokeWidth=1.8;fontColor=#2D2B28;fontSize=18;",
  },
  start: {
    fill: "#F8E9E1",
    stroke: "#D88966",
    text: "#D88966",
    drawio:
      "rounded=1;whiteSpace=wrap;arcSize=50;html=1;fillColor=#F8E9E1;strokeColor=#D88966;strokeWidth=1.8;fontColor=#D88966;fontSize=18;fontStyle=1;",
  },
  end: {
    fill: "#CFE8D7",
    stroke: "#71AE88",
    text: "#2D2B28",
    drawio:
      "rounded=1;whiteSpace=wrap;arcSize=10;html=1;fillColor=#CFE8D7;strokeColor=#71AE88;strokeWidth=1.8;fontColor=#2D2B28;fontSize=18;",
  },
  decision: {
    fill: "#E6D7B4",
    stroke: "#BFA777",
    text: "#2D2B28",
    drawio:
      "rhombus;whiteSpace=wrap;html=1;fillColor=#E6D7B4;strokeColor=#BFA777;strokeWidth=1.8;fontColor=#2D2B28;fontSize=18;",
  },
  ai: {
    fill: "#D7E6DC",
    stroke: "#7FB08F",
    text: "#2D2B28",
    drawio:
      "rounded=1;whiteSpace=wrap;arcSize=10;html=1;fillColor=#D7E6DC;strokeColor=#7FB08F;strokeWidth=1.8;fontColor=#2D2B28;fontSize=18;",
  },
  inactive: {
    fill: "#EFECE6",
    stroke: "#B4AEA6",
    text: "#7A756E",
    drawio:
      "rounded=1;whiteSpace=wrap;arcSize=10;html=1;fillColor=#EFECE6;strokeColor=#B4AEA6;strokeWidth=1.8;fontColor=#7A756E;fontSize=18;",
  },
};

const edgeStyles = {
  primary: {
    stroke: "#7A756E",
    drawio:
      "endArrow=open;endSize=14;edgeStyle=orthogonalEdgeStyle;strokeColor=#7A756E;strokeWidth=1.8;rounded=1;",
  },
  optional: {
    stroke: "#9A948C",
    dasharray: "6 6",
    drawio:
      "endArrow=open;endSize=14;edgeStyle=orthogonalEdgeStyle;strokeColor=#9A948C;strokeWidth=1.6;rounded=1;dashed=1;dashPattern=6 6;",
  },
  feedback: {
    stroke: "#8E8982",
    drawio:
      "endArrow=open;endSize=14;edgeStyle=orthogonalEdgeStyle;strokeColor=#8E8982;strokeWidth=1.8;rounded=1;curved=1;",
  },
  context: {
    stroke: "#7FB08F",
    drawio:
      "endArrow=open;endSize=14;edgeStyle=orthogonalEdgeStyle;strokeColor=#7FB08F;strokeWidth=1.8;rounded=1;",
  },
};

const panelStyle =
  "rounded=1;whiteSpace=wrap;arcSize=6;fillColor=#FAF8F4;strokeColor=#8C867F;strokeWidth=2;fontSize=18;fontStyle=1;fontColor=#5F5A54;swimlane;startSize=54;horizontal=1;html=1;";

const entityStyles = {
  primary:
    "rounded=1;whiteSpace=wrap;arcSize=6;fillColor=#FAF8F4;strokeColor=#8C867F;strokeWidth=1.8;fontSize=16;fontStyle=1;fontColor=#2D2B28;swimlane;startSize=48;horizontal=1;html=1;",
  secondary:
    "rounded=1;whiteSpace=wrap;arcSize=6;fillColor=#F7FBFF;strokeColor=#6FA8D6;strokeWidth=1.8;fontSize=16;fontStyle=1;fontColor=#2D2B28;swimlane;startSize=48;horizontal=1;html=1;",
  tertiary:
    "rounded=1;whiteSpace=wrap;arcSize=6;fillColor=#F7F5FE;strokeColor=#9A90D6;strokeWidth=1.8;fontSize=16;fontStyle=1;fontColor=#2D2B28;swimlane;startSize=48;horizontal=1;html=1;",
  inactive:
    "rounded=1;whiteSpace=wrap;arcSize=6;fillColor=#FAF8F4;strokeColor=#B4AEA6;strokeWidth=1.8;fontSize=16;fontStyle=1;fontColor=#7A756E;swimlane;startSize=48;horizontal=1;html=1;",
};

const diagrams = [
  {
    slug: "figure-3-1",
    title: "图 3-1 人脸考勤系统总体架构图",
    height: 900,
    spec: {
      main_claim:
        "系统以服务端为中枢，连接管理端与 ESP32-S3 设备端，并将核心业务数据持久化到数据库。",
      pattern: "Grouped Architecture",
      secondary_pattern: "none",
      reading_direction: "left-to-right",
    },
    shapes: [
      { type: "border", id: "border", x: 20, y: 20, w: 1614, h: 840 },
      {
        type: "title",
        id: "title",
        x: 80,
        y: 44,
        w: 1494,
        h: 44,
        text: ["图 3-1 人脸考勤系统总体架构图"],
      },
      { type: "panel", id: "panel-admin", x: 80, y: 130, w: 360, h: 300, label: "管理端" },
      { type: "panel", id: "panel-service", x: 510, y: 130, w: 420, h: 380, label: "服务端" },
      { type: "panel", id: "panel-device", x: 1000, y: 130, w: 574, h: 580, label: "设备端" },

      {
        type: "node",
        id: "admin-app",
        semantic: "primary",
        x: 110,
        y: 220,
        w: 300,
        h: 70,
        text: ["Web 管理后台"],
      },
      {
        type: "node",
        id: "admin-pages",
        semantic: "secondary",
        x: 110,
        y: 320,
        w: 135,
        h: 70,
        text: ["业务管理页面"],
      },
      {
        type: "node",
        id: "admin-domains",
        semantic: "secondary",
        x: 275,
        y: 320,
        w: 135,
        h: 70,
        text: ["概览 / 员工 / 设备", "任务 / 考勤"],
      },

      {
        type: "node",
        id: "service-core",
        semantic: "tertiary",
        x: 560,
        y: 220,
        w: 320,
        h: 70,
        text: ["Nuxt + Nitro + oRPC"],
      },
      {
        type: "node",
        id: "service-admin-api",
        semantic: "secondary",
        x: 560,
        y: 320,
        w: 140,
        h: 70,
        text: ["管理端接口"],
      },
      {
        type: "node",
        id: "service-device-api",
        semantic: "secondary",
        x: 740,
        y: 320,
        w: 140,
        h: 70,
        text: ["设备端接口"],
      },
      {
        type: "node",
        id: "service-rules",
        semantic: "tertiary",
        x: 650,
        y: 420,
        w: 140,
        h: 70,
        text: ["业务规则处理"],
      },
      {
        type: "cylinder",
        id: "service-db",
        semantic: "secondary",
        x: 620,
        y: 570,
        w: 200,
        h: 110,
        text: ["SQLite", "数据库"],
      },

      {
        type: "node",
        id: "device-app",
        semantic: "primary",
        x: 1050,
        y: 220,
        w: 470,
        h: 70,
        text: ["ESP32-S3 考勤机应用"],
      },
      {
        type: "node",
        id: "device-config",
        semantic: "secondary",
        x: 1050,
        y: 330,
        w: 210,
        h: 80,
        text: ["USB 首配", "Wi-Fi / HTTP"],
      },
      {
        type: "node",
        id: "device-ai",
        semantic: "ai",
        x: 1310,
        y: 330,
        w: 210,
        h: 80,
        text: ["摄像头", "ESP-WHO / ESP-DL"],
      },
      {
        type: "node",
        id: "device-storage",
        semantic: "secondary",
        x: 1050,
        y: 450,
        w: 210,
        h: 80,
        text: ["NVS / LittleFS", "SD Card / PSRAM"],
      },
      {
        type: "node",
        id: "device-ui",
        semantic: "secondary",
        x: 1310,
        y: 450,
        w: 210,
        h: 80,
        text: ["LVGL 状态界面", "本地交互"],
      },
      {
        type: "node",
        id: "device-runtime",
        semantic: "ai",
        x: 1180,
        y: 580,
        w: 210,
        h: 70,
        text: ["识别 / 缓存 / 上传"],
      },

      {
        type: "edge",
        id: "edge-admin-service",
        semantic: "primary",
        from: "admin-app",
        to: "service-admin-api",
        fromAnchor: "right",
        toAnchor: "left",
        points: [],
      },
      {
        type: "edge",
        id: "edge-device-service",
        semantic: "primary",
        from: "device-runtime",
        to: "service-device-api",
        fromAnchor: "left",
        toAnchor: "right",
        points: [
          { x: 1090, y: 615 },
          { x: 1090, y: 355 },
        ],
      },
      {
        type: "edge",
        id: "edge-service-db",
        semantic: "context",
        from: "service-rules",
        to: "service-db",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },

      {
        type: "text",
        id: "label-admin-service",
        x: 420,
        y: 255,
        w: 120,
        h: 24,
        text: ["业务管理"],
        fontSize: 15,
        fill: "#7A756E",
      },
      {
        type: "text",
        id: "label-device-service",
        x: 930,
        y: 300,
        w: 140,
        h: 24,
        text: ["激活 / 同步 / 上传"],
        fontSize: 15,
        fill: "#7A756E",
      },
      {
        type: "text",
        id: "label-service-db",
        x: 780,
        y: 515,
        w: 80,
        h: 24,
        text: ["持久化"],
        fontSize: 15,
        fill: "#7A756E",
      },
    ],
  },
  {
    slug: "figure-3-2",
    title: "图 3-2 设备建档与激活流程图",
    height: 920,
    spec: {
      main_claim:
        "设备从后台建档到正式运行，需要经历后台建档、首配写入、引导激活和正式凭据下发四个跨端阶段。",
      pattern: "Swimlane Sequence",
      secondary_pattern: "Linear Workflow",
      reading_direction: "left-to-right",
    },
    shapes: [
      { type: "border", id: "border", x: 20, y: 20, w: 1614, h: 860 },
      {
        type: "title",
        id: "title",
        x: 80,
        y: 44,
        w: 1494,
        h: 44,
        text: ["图 3-2 设备建档与激活流程图"],
      },

      { type: "panel", id: "lane-admin", x: 80, y: 140, w: 1494, h: 190, label: "管理端" },
      { type: "panel", id: "lane-service", x: 80, y: 380, w: 1494, h: 190, label: "服务端" },
      { type: "panel", id: "lane-device", x: 80, y: 620, w: 1494, h: 190, label: "设备端" },

      {
        type: "node",
        id: "step-a",
        semantic: "start",
        x: 110,
        y: 210,
        w: 190,
        h: 70,
        text: ["后台创建设备"],
      },
      {
        type: "node",
        id: "step-b",
        semantic: "tertiary",
        x: 360,
        y: 450,
        w: 240,
        h: 70,
        text: ["生成 deviceCode", "与引导身份"],
      },
      {
        type: "node",
        id: "step-c",
        semantic: "warning",
        x: 650,
        y: 200,
        w: 260,
        h: 90,
        text: ["通过 Web Serial / USB", "写入 Wi-Fi、后端地址", "和引导身份"],
      },
      {
        type: "node",
        id: "step-d",
        semantic: "primary",
        x: 960,
        y: 690,
        w: 220,
        h: 70,
        text: ["调用引导激活接口"],
      },
      {
        type: "node",
        id: "step-e",
        semantic: "tertiary",
        x: 1210,
        y: 440,
        w: 250,
        h: 90,
        text: ["校验引导身份", "并返回正式运行凭据"],
      },
      {
        type: "node",
        id: "step-f",
        semantic: "end",
        x: 1310,
        y: 680,
        w: 210,
        h: 90,
        text: ["保存正式凭据", "进入正常运行"],
      },

      {
        type: "edge",
        id: "edge-a-b",
        semantic: "primary",
        from: "step-a",
        to: "step-b",
        fromAnchor: "right",
        toAnchor: "left",
        points: [
          { x: 320, y: 245 },
          { x: 320, y: 485 },
        ],
      },
      {
        type: "edge",
        id: "edge-b-c",
        semantic: "primary",
        from: "step-b",
        to: "step-c",
        fromAnchor: "right",
        toAnchor: "left",
        points: [
          { x: 625, y: 485 },
          { x: 625, y: 245 },
        ],
      },
      {
        type: "edge",
        id: "edge-c-d",
        semantic: "primary",
        from: "step-c",
        to: "step-d",
        fromAnchor: "right",
        toAnchor: "left",
        points: [
          { x: 930, y: 245 },
          { x: 930, y: 725 },
        ],
      },
      {
        type: "edge",
        id: "edge-d-e",
        semantic: "primary",
        from: "step-d",
        to: "step-e",
        fromAnchor: "right",
        toAnchor: "left",
        points: [
          { x: 1195, y: 725 },
          { x: 1195, y: 485 },
        ],
      },
      {
        type: "edge",
        id: "edge-e-f",
        semantic: "primary",
        from: "step-e",
        to: "step-f",
        fromAnchor: "right",
        toAnchor: "left",
        points: [
          { x: 1480, y: 485 },
          { x: 1480, y: 725 },
        ],
      },
    ],
  },
  {
    slug: "figure-3-4",
    title: "图 3-4 数据库关系图",
    height: 980,
    spec: {
      main_claim:
        "数据库以 employee 和 device 为核心主实体，通过 face_profile 与 attendance_record 支撑录入任务和考勤结果闭环。",
      pattern: "Grouped Architecture",
      secondary_pattern: "none",
      reading_direction: "top-to-bottom",
    },
    shapes: [
      { type: "border", id: "border", x: 20, y: 20, w: 1614, h: 920 },
      { type: "title", id: "title", x: 80, y: 44, w: 1494, h: 44, text: ["图 3-4 数据库关系图"] },

      {
        type: "entity",
        id: "db-employee",
        semantic: "primary",
        x: 150,
        y: 170,
        w: 320,
        h: 250,
        title: "employee",
        fields: ["PK id", "UK code", "name", "created_at", "updated_at"],
      },
      {
        type: "entity",
        id: "db-device",
        semantic: "primary",
        x: 1170,
        y: 170,
        w: 320,
        h: 270,
        title: "device",
        fields: [
          "PK id",
          "UK device_code",
          "name",
          "api_key_hash",
          "status",
          "last_seen_at",
          "created_at",
          "updated_at",
        ],
      },
      {
        type: "entity",
        id: "db-face",
        semantic: "secondary",
        x: 520,
        y: 170,
        w: 320,
        h: 260,
        title: "face_profile",
        fields: ["PK id", "FK employee_id", "FK device_id", "status", "created_at", "updated_at"],
      },
      {
        type: "entity",
        id: "db-record",
        semantic: "secondary",
        x: 520,
        y: 560,
        w: 320,
        h: 290,
        title: "attendance_record",
        fields: [
          "PK id",
          "FK employee_id",
          "FK device_id",
          "recognized_at",
          "local_date",
          "type",
          "created_at",
          "updated_at",
        ],
      },
      {
        type: "entity",
        id: "db-config",
        semantic: "inactive",
        x: 960,
        y: 590,
        w: 360,
        h: 220,
        title: "attendance_config",
        fields: [
          "PK id",
          "work_start_minute",
          "work_end_minute",
          "off_start_minute",
          "off_end_minute",
          "updated_at",
        ],
        note: "单例配置，不建立外键",
      },

      {
        type: "edge",
        id: "edge-emp-face",
        semantic: "primary",
        from: "db-employee",
        to: "db-face",
        fromAnchor: "right",
        toAnchor: "left",
        points: [],
        drawioEdgeStyle: "entityRelationEdgeStyle",
        startArrow: "ERone",
        endArrow: "ERzeroToOne",
      },
      {
        type: "edge",
        id: "edge-device-face",
        semantic: "primary",
        from: "db-device",
        to: "db-face",
        fromAnchor: "left",
        toAnchor: "right",
        points: [],
        drawioEdgeStyle: "entityRelationEdgeStyle",
        startArrow: "ERone",
        endArrow: "ERmany",
      },
      {
        type: "edge",
        id: "edge-emp-record",
        semantic: "context",
        from: "db-employee",
        to: "db-record",
        fromAnchor: "bottom",
        toAnchor: "left",
        points: [
          { x: 310, y: 500 },
          { x: 310, y: 705 },
        ],
        drawioEdgeStyle: "entityRelationEdgeStyle",
        startArrow: "ERone",
        endArrow: "ERmany",
      },
      {
        type: "edge",
        id: "edge-device-record",
        semantic: "context",
        from: "db-device",
        to: "db-record",
        fromAnchor: "bottom",
        toAnchor: "right",
        points: [
          { x: 1330, y: 520 },
          { x: 1330, y: 705 },
        ],
        drawioEdgeStyle: "entityRelationEdgeStyle",
        startArrow: "ERone",
        endArrow: "ERmany",
      },

      {
        type: "text",
        id: "label-emp-face-name",
        x: 418,
        y: 252,
        w: 72,
        h: 24,
        text: ["has"],
        fontSize: 15,
        fill: "#7A756E",
      },
      {
        type: "text",
        id: "label-device-face-name",
        x: 904,
        y: 252,
        w: 88,
        h: 24,
        text: ["handles"],
        fontSize: 15,
        fill: "#7A756E",
      },
      {
        type: "text",
        id: "label-emp-record-name",
        x: 252,
        y: 688,
        w: 80,
        h: 24,
        text: ["owns"],
        fontSize: 15,
        fill: "#7A756E",
      },
      {
        type: "text",
        id: "label-device-record-name",
        x: 1222,
        y: 688,
        w: 90,
        h: 24,
        text: ["uploads"],
        fontSize: 15,
        fill: "#7A756E",
      },
    ],
  },
  {
    slug: "figure-3-5",
    title: "图 3-5 系统模块划分图",
    height: 960,
    spec: {
      main_claim:
        "系统由五个功能模块组成，其中设备运行与缓存模块承接现场执行，其他模块围绕配置、管理和结果闭环协同工作。",
      pattern: "Hub-and-Spoke",
      secondary_pattern: "Grouped Architecture",
      reading_direction: "left-to-right",
    },
    shapes: [
      { type: "border", id: "border", x: 20, y: 20, w: 1614, h: 900 },
      { type: "title", id: "title", x: 80, y: 44, w: 1494, h: 44, text: ["图 3-5 系统模块划分图"] },

      {
        type: "panel",
        id: "module-panel",
        x: 100,
        y: 140,
        w: 1450,
        h: 720,
        label: "人脸考勤系统功能模块",
      },

      {
        type: "node",
        id: "mod-access",
        semantic: "start",
        x: 170,
        y: 250,
        w: 250,
        h: 110,
        text: ["设备接入模块", "设备建档", "首配 / 激活 / 身份确认"],
      },
      {
        type: "node",
        id: "mod-manage",
        semantic: "primary",
        x: 170,
        y: 570,
        w: 250,
        h: 110,
        text: ["人员与设备管理模块", "员工维护 / 设备维护", "基础状态管理"],
      },
      {
        type: "node",
        id: "mod-rules",
        semantic: "tertiary",
        x: 610,
        y: 190,
        w: 430,
        h: 130,
        text: ["考勤规则与任务模块", "考勤时间配置", "人脸录入任务下发与取消"],
      },
      {
        type: "node",
        id: "mod-runtime",
        semantic: "ai",
        x: 610,
        y: 470,
        w: 430,
        h: 150,
        text: [
          "设备运行与缓存模块",
          "本地快照 / 离线队列 / 失败日志",
          "摄像头采集 / 识别 / 本地判定",
        ],
      },
      {
        type: "node",
        id: "mod-query",
        semantic: "end",
        x: 1180,
        y: 360,
        w: 280,
        h: 130,
        text: ["数据上传与查询模块", "考勤记录回传 / 入库", "统计查询与结果展示"],
      },

      {
        type: "edge",
        id: "edge-access-runtime",
        semantic: "primary",
        from: "mod-access",
        to: "mod-runtime",
        fromAnchor: "right",
        toAnchor: "left",
        points: [{ x: 500, y: 305 }],
      },
      {
        type: "edge",
        id: "edge-manage-rules",
        semantic: "context",
        from: "mod-manage",
        to: "mod-rules",
        fromAnchor: "right",
        toAnchor: "left",
        points: [
          { x: 520, y: 625 },
          { x: 520, y: 255 },
        ],
      },
      {
        type: "edge",
        id: "edge-rules-runtime",
        semantic: "primary",
        from: "mod-rules",
        to: "mod-runtime",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-runtime-query",
        semantic: "primary",
        from: "mod-runtime",
        to: "mod-query",
        fromAnchor: "right",
        toAnchor: "left",
        points: [],
      },
      {
        type: "edge",
        id: "edge-manage-query",
        semantic: "optional",
        from: "mod-manage",
        to: "mod-query",
        fromAnchor: "right",
        toAnchor: "bottom",
        points: [
          { x: 1080, y: 625 },
          { x: 1080, y: 555 },
        ],
      },

      {
        type: "text",
        id: "label-access-runtime",
        x: 470,
        y: 285,
        w: 120,
        h: 24,
        text: ["接入完成后进入运行"],
        fontSize: 15,
        fill: "#7A756E",
      },
      {
        type: "text",
        id: "label-manage-rules",
        x: 450,
        y: 430,
        w: 120,
        h: 40,
        text: ["基础数据", "任务来源"],
        fontSize: 15,
        fill: "#7A756E",
      },
      {
        type: "text",
        id: "label-rules-runtime",
        x: 800,
        y: 385,
        w: 80,
        h: 24,
        text: ["同步执行"],
        fontSize: 15,
        fill: "#7A756E",
      },
      {
        type: "text",
        id: "label-runtime-query",
        x: 1060,
        y: 515,
        w: 100,
        h: 24,
        text: ["上传与展示"],
        fontSize: 15,
        fill: "#7A756E",
      },
    ],
  },
  {
    slug: "figure-3-6",
    title: "图 3-6 设备端分层存储结构图",
    height: 980,
    spec: {
      main_claim:
        "设备端对不同类型数据采用分层存储策略，其中 NVS 负责关键身份，LittleFS 负责业务缓存，SD Card 负责容量型数据，PSRAM 负责运行期加载。",
      pattern: "Layered Stack",
      secondary_pattern: "Grouped Architecture",
      reading_direction: "top-to-bottom",
    },
    shapes: [
      { type: "border", id: "border", x: 20, y: 20, w: 1614, h: 920 },
      {
        type: "title",
        id: "title",
        x: 80,
        y: 44,
        w: 1494,
        h: 44,
        text: ["图 3-6 设备端分层存储结构图"],
      },
      {
        type: "panel",
        id: "storage-panel",
        x: 190,
        y: 130,
        w: 1270,
        h: 760,
        label: "设备端分层存储架构",
      },

      {
        type: "node",
        id: "storage-top",
        semantic: "start",
        x: 470,
        y: 180,
        w: 700,
        h: 90,
        text: ["设备运行主循环", "读取凭据 / 同步数据 / 本地识别 / 上传结果"],
      },
      {
        type: "node",
        id: "storage-nvs",
        semantic: "primary",
        x: 290,
        y: 320,
        w: 1060,
        h: 100,
        text: [
          "NVS / Preferences",
          "保存设备身份、设备凭据和少量关键配置",
          "目标：设备重启后可恢复核心运行信息",
        ],
      },
      {
        type: "node",
        id: "storage-lfs",
        semantic: "secondary",
        x: 290,
        y: 460,
        w: 1060,
        h: 110,
        text: [
          "LittleFS",
          "保存员工快照、考勤配置、任务列表、待上传队列和失败日志",
          "目标：保障断网场景下的最小业务闭环",
        ],
      },
      {
        type: "node",
        id: "storage-sd",
        semantic: "tertiary",
        x: 290,
        y: 620,
        w: 1060,
        h: 100,
        text: ["SD Card", "保存人脸特征数据及其他容量型扩展文件", "目标：承担大容量持久化存储"],
      },
      {
        type: "node",
        id: "storage-psram",
        semantic: "ai",
        x: 290,
        y: 770,
        w: 1060,
        h: 80,
        text: ["PSRAM", "运行期加载人脸特征和识别相关数据，提升处理效率"],
      },

      {
        type: "edge",
        id: "edge-top-nvs",
        semantic: "primary",
        from: "storage-top",
        to: "storage-nvs",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-nvs-lfs",
        semantic: "context",
        from: "storage-nvs",
        to: "storage-lfs",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-lfs-sd",
        semantic: "context",
        from: "storage-lfs",
        to: "storage-sd",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-sd-psram",
        semantic: "primary",
        from: "storage-sd",
        to: "storage-psram",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },

      {
        type: "text",
        id: "label-top-nvs",
        x: 1150,
        y: 285,
        w: 110,
        h: 24,
        text: ["启动恢复"],
        fontSize: 15,
        fill: "#7A756E",
      },
      {
        type: "text",
        id: "label-nvs-lfs",
        x: 1150,
        y: 435,
        w: 110,
        h: 24,
        text: ["配置驱动"],
        fontSize: 15,
        fill: "#7A756E",
      },
      {
        type: "text",
        id: "label-lfs-sd",
        x: 1150,
        y: 595,
        w: 110,
        h: 24,
        text: ["缓存协同"],
        fontSize: 15,
        fill: "#7A756E",
      },
      {
        type: "text",
        id: "label-sd-psram",
        x: 1150,
        y: 745,
        w: 110,
        h: 24,
        text: ["运行加载"],
        fontSize: 15,
        fill: "#7A756E",
      },
    ],
  },
  {
    slug: "figure-3-3",
    title: "图 3-3 设备日常运行流程图",
    height: 1320,
    spec: {
      main_claim:
        "设备日常运行以本地识别和离线缓存为核心，网络可用时再与服务端同步和上传，后台最终负责查询展示。",
      pattern: "Feedback Loop Workflow",
      secondary_pattern: "Grouped Architecture",
      reading_direction: "top-to-bottom",
    },
    shapes: [
      { type: "border", id: "border", x: 20, y: 20, w: 1614, h: 1260 },
      {
        type: "title",
        id: "title",
        x: 80,
        y: 44,
        w: 1494,
        h: 44,
        text: ["图 3-3 设备日常运行流程图"],
      },

      { type: "panel", id: "panel-device", x: 80, y: 140, w: 760, h: 1080, label: "设备端" },
      { type: "panel", id: "panel-service", x: 880, y: 140, w: 400, h: 1140, label: "服务端" },
      { type: "panel", id: "panel-admin", x: 1330, y: 940, w: 244, h: 240, label: "管理端" },

      {
        type: "node",
        id: "daily-a",
        semantic: "start",
        x: 130,
        y: 230,
        w: 280,
        h: 80,
        text: ["设备启动并加载", "本地数据"],
      },
      {
        type: "node",
        id: "daily-b",
        semantic: "primary",
        x: 130,
        y: 370,
        w: 280,
        h: 80,
        text: ["连接 Wi-Fi 并", "检查激活状态"],
      },
      {
        type: "node",
        id: "daily-c",
        semantic: "primary",
        x: 130,
        y: 510,
        w: 280,
        h: 80,
        text: ["请求业务同步"],
      },
      {
        type: "node",
        id: "daily-d",
        semantic: "secondary",
        x: 940,
        y: 500,
        w: 300,
        h: 90,
        text: ["返回考勤配置、员工快照", "和录入任务"],
      },
      {
        type: "node",
        id: "daily-e",
        semantic: "ai",
        x: 130,
        y: 700,
        w: 280,
        h: 110,
        text: ["摄像头采集", "人脸检测 / 特征生成", "本地识别"],
      },
      {
        type: "diamond",
        id: "daily-f",
        semantic: "decision",
        x: 185,
        y: 860,
        w: 170,
        h: 120,
        text: ["识别结果", "是否有效"],
      },
      {
        type: "node",
        id: "daily-g",
        semantic: "primary",
        x: 130,
        y: 1010,
        w: 280,
        h: 80,
        text: ["生成考勤记录并", "写入离线队列"],
      },
      {
        type: "diamond",
        id: "daily-h",
        semantic: "decision",
        x: 185,
        y: 1120,
        w: 170,
        h: 120,
        text: ["网络是否", "可用"],
      },
      {
        type: "node",
        id: "daily-i",
        semantic: "inactive",
        x: 480,
        y: 1100,
        w: 260,
        h: 90,
        text: ["等待网络恢复后", "批量上传"],
      },
      {
        type: "node",
        id: "daily-j",
        semantic: "secondary",
        x: 940,
        y: 1000,
        w: 300,
        h: 90,
        text: ["上传考勤记录", "POST /api/device/attendance/upload"],
      },
      {
        type: "node",
        id: "daily-k",
        semantic: "tertiary",
        x: 940,
        y: 1150,
        w: 300,
        h: 90,
        text: ["服务端入库、去重", "与更早时间覆盖"],
      },
      {
        type: "node",
        id: "daily-l",
        semantic: "end",
        x: 1370,
        y: 1015,
        w: 160,
        h: 90,
        text: ["后台查询结果", "与任务状态"],
      },

      {
        type: "edge",
        id: "edge-daily-a-b",
        semantic: "primary",
        from: "daily-a",
        to: "daily-b",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-daily-b-c",
        semantic: "primary",
        from: "daily-b",
        to: "daily-c",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-daily-c-d",
        semantic: "primary",
        from: "daily-c",
        to: "daily-d",
        fromAnchor: "right",
        toAnchor: "left",
        points: [{ x: 600, y: 550 }],
      },
      {
        type: "edge",
        id: "edge-daily-d-e",
        semantic: "context",
        from: "daily-d",
        to: "daily-e",
        fromAnchor: "left",
        toAnchor: "right",
        points: [{ x: 600, y: 755 }],
      },
      {
        type: "edge",
        id: "edge-daily-e-f",
        semantic: "primary",
        from: "daily-e",
        to: "daily-f",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-daily-f-g",
        semantic: "primary",
        from: "daily-f",
        to: "daily-g",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-daily-f-e",
        semantic: "feedback",
        from: "daily-f",
        to: "daily-e",
        fromAnchor: "left",
        toAnchor: "left",
        points: [
          { x: 110, y: 920 },
          { x: 110, y: 755 },
        ],
      },
      {
        type: "edge",
        id: "edge-daily-g-h",
        semantic: "primary",
        from: "daily-g",
        to: "daily-h",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-daily-h-i",
        semantic: "optional",
        from: "daily-h",
        to: "daily-i",
        fromAnchor: "right",
        toAnchor: "left",
        points: [{ x: 420, y: 1165 }],
      },
      {
        type: "edge",
        id: "edge-daily-h-j",
        semantic: "primary",
        from: "daily-h",
        to: "daily-j",
        fromAnchor: "right",
        toAnchor: "left",
        points: [{ x: 600, y: 1180 }],
      },
      {
        type: "edge",
        id: "edge-daily-i-j",
        semantic: "optional",
        from: "daily-i",
        to: "daily-j",
        fromAnchor: "right",
        toAnchor: "left",
        points: [{ x: 820, y: 1145 }],
      },
      {
        type: "edge",
        id: "edge-daily-j-k",
        semantic: "primary",
        from: "daily-j",
        to: "daily-k",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-daily-k-l",
        semantic: "context",
        from: "daily-k",
        to: "daily-l",
        fromAnchor: "right",
        toAnchor: "left",
        points: [
          { x: 1310, y: 1195 },
          { x: 1310, y: 1060 },
        ],
      },

      {
        type: "text",
        id: "label-f-yes",
        x: 365,
        y: 905,
        w: 40,
        h: 24,
        text: ["是"],
        fontSize: 15,
        fill: "#7A756E",
      },
      {
        type: "text",
        id: "label-f-no",
        x: 95,
        y: 895,
        w: 40,
        h: 24,
        text: ["否"],
        fontSize: 15,
        fill: "#7A756E",
      },
      {
        type: "text",
        id: "label-h-no",
        x: 425,
        y: 1148,
        w: 40,
        h: 24,
        text: ["否"],
        fontSize: 15,
        fill: "#7A756E",
      },
      {
        type: "text",
        id: "label-h-yes",
        x: 620,
        y: 1158,
        w: 40,
        h: 24,
        text: ["是"],
        fontSize: 15,
        fill: "#7A756E",
      },
    ],
  },
  {
    slug: "figure-4-1",
    title: "图 4-1 管理后台页面结构图",
    height: 980,
    spec: {
      main_claim:
        "后台以前台仪表盘和统一导航为入口，围绕员工、设备、录脸记录、考勤记录和串口配置形成完整页面结构。",
      pattern: "Hub-and-Spoke",
      secondary_pattern: "Grouped Architecture",
      reading_direction: "left-to-right",
    },
    shapes: [
      { type: "border", id: "border", x: 20, y: 20, w: 1614, h: 920 },
      {
        type: "title",
        id: "title",
        x: 80,
        y: 44,
        w: 1494,
        h: 44,
        text: ["图 4-1 管理后台页面结构图"],
      },
      {
        type: "panel",
        id: "admin-pages-panel",
        x: 120,
        y: 130,
        w: 1410,
        h: 760,
        label: "Nuxt 管理后台页面结构",
      },
      {
        type: "node",
        id: "admin-root",
        semantic: "tertiary",
        x: 540,
        y: 160,
        w: 580,
        h: 90,
        text: [
          "统一入口：考勤管理后台",
          "dashboard layout + auth middleware + PageHeader / DataSurface",
        ],
      },
      {
        type: "node",
        id: "admin-dashboard",
        semantic: "start",
        x: 610,
        y: 320,
        w: 440,
        h: 110,
        text: ["概览页 /dashboard", "统计卡片、考勤配置、快捷入口"],
      },
      {
        type: "node",
        id: "admin-employees",
        semantic: "primary",
        x: 180,
        y: 520,
        w: 260,
        h: 110,
        text: ["员工管理 /employees", "员工列表、筛选", "新增 / 编辑员工"],
      },
      {
        type: "node",
        id: "admin-devices",
        semantic: "primary",
        x: 500,
        y: 520,
        w: 260,
        h: 110,
        text: ["设备管理 /devices", "设备列表、创建结果", "启用 / 禁用 / 删除"],
      },
      {
        type: "node",
        id: "admin-face",
        semantic: "primary",
        x: 820,
        y: 520,
        w: 260,
        h: 110,
        text: ["录脸记录 /face-profiles", "待录入任务列表", "分配任务 / 取消任务"],
      },
      {
        type: "node",
        id: "admin-attendance",
        semantic: "primary",
        x: 1140,
        y: 520,
        w: 260,
        h: 110,
        text: ["考勤记录 /attendance-records", "按日期、设备、员工筛选", "查询打卡结果"],
      },
      {
        type: "node",
        id: "admin-serial",
        semantic: "secondary",
        x: 500,
        y: 700,
        w: 260,
        h: 110,
        text: ["串口配置 /devices/serial", "Web Serial 连接设备", "写入配置并激活"],
      },
      {
        type: "node",
        id: "admin-components",
        semantic: "secondary",
        x: 1020,
        y: 700,
        w: 340,
        h: 110,
        text: [
          "关键共享组件",
          "AttendanceConfigEditor / DeviceSerialWorkspace",
          "FilterBar / MetricStrip / ListPagination",
        ],
      },

      {
        type: "edge",
        id: "edge-admin-root-dashboard",
        semantic: "primary",
        from: "admin-root",
        to: "admin-dashboard",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-admin-dashboard-employees",
        semantic: "primary",
        from: "admin-dashboard",
        to: "admin-employees",
        fromAnchor: "left",
        toAnchor: "top",
        points: [{ x: 380, y: 470 }],
      },
      {
        type: "edge",
        id: "edge-admin-dashboard-devices",
        semantic: "primary",
        from: "admin-dashboard",
        to: "admin-devices",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-admin-dashboard-face",
        semantic: "primary",
        from: "admin-dashboard",
        to: "admin-face",
        fromAnchor: "right",
        toAnchor: "top",
        points: [{ x: 950, y: 470 }],
      },
      {
        type: "edge",
        id: "edge-admin-dashboard-attendance",
        semantic: "primary",
        from: "admin-dashboard",
        to: "admin-attendance",
        fromAnchor: "right",
        toAnchor: "top",
        points: [{ x: 1280, y: 470 }],
      },
      {
        type: "edge",
        id: "edge-admin-devices-serial",
        semantic: "context",
        from: "admin-devices",
        to: "admin-serial",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-admin-pages-components",
        semantic: "optional",
        from: "admin-face",
        to: "admin-components",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [{ x: 1100, y: 675 }],
      },
      {
        type: "edge",
        id: "edge-admin-attendance-components",
        semantic: "optional",
        from: "admin-attendance",
        to: "admin-components",
        fromAnchor: "bottom",
        toAnchor: "right",
        points: [{ x: 1360, y: 675 }],
      },
    ],
  },
  {
    slug: "figure-4-2",
    title: "图 4-2 串口配置与设备激活交互图",
    height: 1120,
    spec: {
      main_claim:
        "设备首配通过管理员、Web Serial 页面、设备和后台服务协同完成，串口写入与 bootstrap 激活构成完整闭环。",
      pattern: "Swimlane Sequence",
      secondary_pattern: "Linear Workflow",
      reading_direction: "top-to-bottom",
    },
    shapes: [
      { type: "border", id: "border", x: 20, y: 20, w: 1614, h: 1060 },
      {
        type: "title",
        id: "title",
        x: 80,
        y: 44,
        w: 1494,
        h: 44,
        text: ["图 4-2 串口配置与设备激活交互图"],
      },
      { type: "panel", id: "lane-operator", x: 90, y: 140, w: 280, h: 870, label: "管理员" },
      { type: "panel", id: "lane-browser", x: 400, y: 140, w: 340, h: 870, label: "浏览器串口页" },
      { type: "panel", id: "lane-device", x: 770, y: 140, w: 340, h: 870, label: "设备" },
      { type: "panel", id: "lane-server", x: 1140, y: 140, w: 340, h: 870, label: "后台服务" },

      {
        type: "node",
        id: "serial-a",
        semantic: "start",
        x: 120,
        y: 210,
        w: 220,
        h: 76,
        text: ["创建设备或", "选择待激活设备"],
      },
      {
        type: "node",
        id: "serial-b",
        semantic: "primary",
        x: 460,
        y: 210,
        w: 220,
        h: 76,
        text: ["请求串口连接", "并读取 get_config"],
      },
      {
        type: "node",
        id: "serial-c",
        semantic: "primary",
        x: 830,
        y: 210,
        w: 220,
        h: 76,
        text: ["返回当前配置摘要", "backendOrigin / Wi-Fi / activationState"],
      },

      {
        type: "node",
        id: "serial-d",
        semantic: "secondary",
        x: 1200,
        y: 350,
        w: 220,
        h: 86,
        text: ["查询待激活设备", "device.create / listPendingActivations", "findByBootstrapSerial"],
      },
      {
        type: "node",
        id: "serial-e",
        semantic: "secondary",
        x: 460,
        y: 350,
        w: 220,
        h: 86,
        text: ["填写并校验首配参数", "Wi-Fi / backend origin", "bootstrap identity"],
      },
      {
        type: "node",
        id: "serial-f",
        semantic: "context",
        x: 830,
        y: 350,
        w: 220,
        h: 86,
        text: ["写入设备配置", "set_wifi_profiles", "set_backend_origin / set_bootstrap_identity"],
      },

      {
        type: "node",
        id: "serial-g",
        semantic: "ai",
        x: 830,
        y: 520,
        w: 220,
        h: 86,
        text: ["连接 Wi-Fi", "自动发起 bootstrap/hello"],
      },
      {
        type: "node",
        id: "serial-h",
        semantic: "tertiary",
        x: 1200,
        y: 520,
        w: 220,
        h: 86,
        text: ["校验 bootstrap 身份", "签发 runtime deviceCode / apiKey"],
      },

      {
        type: "node",
        id: "serial-i",
        semantic: "end",
        x: 830,
        y: 690,
        w: 220,
        h: 86,
        text: ["保存正式运行凭据", "转入 sync / upload 正常链路"],
      },
      {
        type: "node",
        id: "serial-j",
        semantic: "end",
        x: 460,
        y: 690,
        w: 220,
        h: 86,
        text: ["显示激活结果", "日志与设备状态可继续查看"],
      },
      {
        type: "node",
        id: "serial-k",
        semantic: "end",
        x: 120,
        y: 690,
        w: 220,
        h: 86,
        text: ["完成首配", "返回设备管理或继续调试"],
      },

      {
        type: "edge",
        id: "edge-serial-a-b",
        semantic: "primary",
        from: "serial-a",
        to: "serial-b",
        fromAnchor: "right",
        toAnchor: "left",
        points: [],
      },
      {
        type: "edge",
        id: "edge-serial-b-c",
        semantic: "primary",
        from: "serial-b",
        to: "serial-c",
        fromAnchor: "right",
        toAnchor: "left",
        points: [],
      },
      {
        type: "edge",
        id: "edge-serial-c-e",
        semantic: "context",
        from: "serial-c",
        to: "serial-e",
        fromAnchor: "left",
        toAnchor: "right",
        points: [{ x: 750, y: 392 }],
      },
      {
        type: "edge",
        id: "edge-serial-a-d",
        semantic: "optional",
        from: "serial-a",
        to: "serial-d",
        fromAnchor: "right",
        toAnchor: "left",
        points: [{ x: 1120, y: 248 }],
      },
      {
        type: "edge",
        id: "edge-serial-e-f",
        semantic: "primary",
        from: "serial-e",
        to: "serial-f",
        fromAnchor: "right",
        toAnchor: "left",
        points: [],
      },
      {
        type: "edge",
        id: "edge-serial-d-e",
        semantic: "optional",
        from: "serial-d",
        to: "serial-e",
        fromAnchor: "left",
        toAnchor: "right",
        points: [{ x: 1120, y: 393 }],
      },
      {
        type: "edge",
        id: "edge-serial-f-g",
        semantic: "primary",
        from: "serial-f",
        to: "serial-g",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-serial-g-h",
        semantic: "primary",
        from: "serial-g",
        to: "serial-h",
        fromAnchor: "right",
        toAnchor: "left",
        points: [],
      },
      {
        type: "edge",
        id: "edge-serial-h-i",
        semantic: "primary",
        from: "serial-h",
        to: "serial-i",
        fromAnchor: "left",
        toAnchor: "right",
        points: [{ x: 1120, y: 733 }],
      },
      {
        type: "edge",
        id: "edge-serial-i-j",
        semantic: "context",
        from: "serial-i",
        to: "serial-j",
        fromAnchor: "left",
        toAnchor: "right",
        points: [],
      },
      {
        type: "edge",
        id: "edge-serial-j-k",
        semantic: "primary",
        from: "serial-j",
        to: "serial-k",
        fromAnchor: "left",
        toAnchor: "right",
        points: [],
      },

      {
        type: "text",
        id: "label-serial-get",
        x: 700,
        y: 230,
        w: 60,
        h: 24,
        text: ["读取"],
        fontSize: 15,
        fill: "#7A756E",
      },
      {
        type: "text",
        id: "label-serial-write",
        x: 700,
        y: 373,
        w: 60,
        h: 24,
        text: ["写入"],
        fontSize: 15,
        fill: "#7A756E",
      },
      {
        type: "text",
        id: "label-serial-hello",
        x: 1080,
        y: 543,
        w: 50,
        h: 24,
        text: ["激活"],
        fontSize: 15,
        fill: "#7A756E",
      },
    ],
  },
  {
    slug: "figure-4-3",
    title: "图 4-3 软件平台接口组织图",
    height: 1080,
    spec: {
      main_claim:
        "软件平台同时承接管理端 oRPC 路由和设备端 HTTP 接口，两类入口共用同一套业务域和数据库。",
      pattern: "Grouped Architecture",
      secondary_pattern: "Layered Stack",
      reading_direction: "left-to-right",
    },
    shapes: [
      { type: "border", id: "border", x: 20, y: 20, w: 1614, h: 1020 },
      {
        type: "title",
        id: "title",
        x: 80,
        y: 44,
        w: 1494,
        h: 44,
        text: ["图 4-3 软件平台接口组织图"],
      },
      { type: "panel", id: "api-left", x: 90, y: 140, w: 360, h: 840, label: "前台调用入口" },
      { type: "panel", id: "api-center", x: 500, y: 140, w: 520, h: 840, label: "服务端接口组织" },
      {
        type: "panel",
        id: "api-right",
        x: 1070,
        y: 140,
        w: 470,
        h: 840,
        label: "业务规则与数据层",
      },

      {
        type: "node",
        id: "api-pages",
        semantic: "primary",
        x: 130,
        y: 220,
        w: 280,
        h: 140,
        text: [
          "管理后台页面",
          "dashboard / employees / devices",
          "face-profiles / attendance-records",
          "devices/serial",
        ],
      },
      {
        type: "node",
        id: "api-device-runtime",
        semantic: "ai",
        x: 130,
        y: 620,
        w: 280,
        h: 140,
        text: [
          "设备端运行时",
          "/api/device/sync",
          "/api/device/enrollment/report",
          "/api/device/attendance/upload",
          "bootstrap/hello",
        ],
      },

      {
        type: "node",
        id: "api-orpc",
        semantic: "tertiary",
        x: 560,
        y: 220,
        w: 400,
        h: 170,
        text: [
          "管理端 oRPC 路由",
          "dashboard / employee / device",
          "attendanceConfig / faceProfile / attendanceRecord",
        ],
      },
      {
        type: "node",
        id: "api-http",
        semantic: "secondary",
        x: 560,
        y: 620,
        w: 400,
        h: 150,
        text: [
          "设备端 HTTP JSON 接口",
          "bootstrap / sync / enrollment/report",
          "attendance/upload",
        ],
      },
      {
        type: "node",
        id: "api-errors",
        semantic: "secondary",
        x: 610,
        y: 830,
        w: 300,
        h: 90,
        text: ["统一响应与错误码", "AdminBusinessError / device error.code / retryable"],
      },

      {
        type: "node",
        id: "api-rules",
        semantic: "primary",
        x: 1130,
        y: 220,
        w: 350,
        h: 150,
        text: ["业务规则层", "登录鉴权 / 设备鉴权", "时间段判定 / 状态流转 / 去重规则"],
      },
      {
        type: "entity",
        id: "api-db",
        semantic: "secondary",
        x: 1130,
        y: 470,
        w: 350,
        h: 280,
        title: "SQLite / Drizzle 业务表",
        fields: ["employee", "device", "attendance_config", "face_profile", "attendance_record"],
      },
      {
        type: "node",
        id: "api-router-index",
        semantic: "inactive",
        x: 1160,
        y: 830,
        w: 290,
        h: 90,
        text: ["appRouter 聚合入口", "packages/api/src/routers/index.ts"],
      },

      {
        type: "edge",
        id: "edge-api-pages-orpc",
        semantic: "primary",
        from: "api-pages",
        to: "api-orpc",
        fromAnchor: "right",
        toAnchor: "left",
        points: [],
      },
      {
        type: "edge",
        id: "edge-api-runtime-http",
        semantic: "primary",
        from: "api-device-runtime",
        to: "api-http",
        fromAnchor: "right",
        toAnchor: "left",
        points: [],
      },
      {
        type: "edge",
        id: "edge-api-orpc-rules",
        semantic: "primary",
        from: "api-orpc",
        to: "api-rules",
        fromAnchor: "right",
        toAnchor: "left",
        points: [],
      },
      {
        type: "edge",
        id: "edge-api-http-rules",
        semantic: "context",
        from: "api-http",
        to: "api-rules",
        fromAnchor: "right",
        toAnchor: "bottom",
        points: [
          { x: 1060, y: 695 },
          { x: 1060, y: 390 },
        ],
      },
      {
        type: "edge",
        id: "edge-api-rules-db",
        semantic: "primary",
        from: "api-rules",
        to: "api-db",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-api-errors-orpc",
        semantic: "optional",
        from: "api-errors",
        to: "api-orpc",
        fromAnchor: "top",
        toAnchor: "bottom",
        points: [],
      },
      {
        type: "edge",
        id: "edge-api-errors-http",
        semantic: "optional",
        from: "api-errors",
        to: "api-http",
        fromAnchor: "top",
        toAnchor: "bottom",
        points: [],
      },
      {
        type: "edge",
        id: "edge-api-index-orpc",
        semantic: "optional",
        from: "api-router-index",
        to: "api-orpc",
        fromAnchor: "left",
        toAnchor: "right",
        points: [{ x: 1030, y: 875 }],
      },
    ],
  },
  {
    slug: "figure-5-1",
    title: "图 5-1 设备端软件分层结构图",
    height: 1040,
    spec: {
      main_claim:
        "设备端围绕 ui、app、core、infra 四层组织，face 作为相机和识别端口侧边接入，PlatformIO / ESP-IDF / Arduino 提供运行底座。",
      pattern: "Layered Stack",
      secondary_pattern: "Grouped Architecture",
      reading_direction: "top-to-bottom",
    },
    shapes: [
      { type: "border", id: "border", x: 20, y: 20, w: 1614, h: 980 },
      {
        type: "title",
        id: "title",
        x: 80,
        y: 44,
        w: 1494,
        h: 44,
        text: ["图 5-1 设备端软件分层结构图"],
      },
      {
        type: "panel",
        id: "dev-layers-panel",
        x: 170,
        y: 130,
        w: 1080,
        h: 820,
        label: "ESP32-S3 设备端分层实现",
      },
      {
        type: "panel",
        id: "dev-side-panel",
        x: 1290,
        y: 130,
        w: 240,
        h: 820,
        label: "端口与运行底座",
      },

      {
        type: "node",
        id: "layer-ui",
        semantic: "start",
        x: 280,
        y: 200,
        w: 860,
        h: 110,
        text: ["ui 层", "StatusScreenPresenter / AppViewModel", "LVGL 状态界面与交互表达"],
      },
      {
        type: "node",
        id: "layer-app",
        semantic: "tertiary",
        x: 280,
        y: 350,
        w: 860,
        h: 140,
        text: [
          "app 层",
          "AppRuntime / runtime_lifecycle_service / runtime_network_executor",
          "系统初始化、网络调度、同步、上传、生命周期编排",
        ],
      },
      {
        type: "node",
        id: "layer-core",
        semantic: "primary",
        x: 280,
        y: 530,
        w: 860,
        h: 120,
        text: ["core 层", "models / use_cases", "考勤类型判定、队列处理、业务规则"],
      },
      {
        type: "node",
        id: "layer-infra",
        semantic: "secondary",
        x: 280,
        y: 690,
        w: 860,
        h: 160,
        text: [
          "infra 层",
          "display / network / storage / board / camera",
          "本地存储、HTTP 设备接口、显示适配、模板存储、板级适配",
        ],
      },

      {
        type: "node",
        id: "layer-face",
        semantic: "ai",
        x: 1330,
        y: 240,
        w: 160,
        h: 130,
        text: ["face 端口", "CameraPort", "EnrollmentServicePort", "RecognitionServicePort"],
      },
      {
        type: "node",
        id: "layer-platform",
        semantic: "inactive",
        x: 1330,
        y: 560,
        w: 160,
        h: 180,
        text: [
          "运行底座",
          "PlatformIO",
          "ESP-IDF primary",
          "Arduino as component",
          "SZPI ESP32-S3 板级环境",
        ],
      },

      {
        type: "edge",
        id: "edge-layer-ui-app",
        semantic: "primary",
        from: "layer-ui",
        to: "layer-app",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-layer-app-core",
        semantic: "primary",
        from: "layer-app",
        to: "layer-core",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-layer-core-infra",
        semantic: "primary",
        from: "layer-core",
        to: "layer-infra",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-layer-face-app",
        semantic: "context",
        from: "layer-face",
        to: "layer-app",
        fromAnchor: "left",
        toAnchor: "right",
        points: [],
      },
      {
        type: "edge",
        id: "edge-layer-platform-infra",
        semantic: "context",
        from: "layer-platform",
        to: "layer-infra",
        fromAnchor: "left",
        toAnchor: "right",
        points: [{ x: 1260, y: 770 }],
      },
    ],
  },
  {
    slug: "figure-5-2",
    title: "图 5-2 设备端初始化流程图",
    height: 900,
    spec: {
      main_claim:
        "设备端在 setup 阶段依次完成本地状态、网络、模板存储、相机与人脸引擎初始化，并在首轮更新后进入稳定运行。",
      pattern: "Linear Workflow",
      secondary_pattern: "Grouped Architecture",
      reading_direction: "left-to-right",
    },
    shapes: [
      { type: "border", id: "border", x: 20, y: 20, w: 1614, h: 840 },
      {
        type: "title",
        id: "title",
        x: 80,
        y: 44,
        w: 1494,
        h: 44,
        text: ["图 5-2 设备端初始化流程图"],
      },
      {
        type: "panel",
        id: "runtime-setup-panel",
        x: 100,
        y: 140,
        w: 1450,
        h: 650,
        label: "setup() 初始化阶段",
      },
      {
        type: "node",
        id: "runtime-setup-a",
        semantic: "start",
        x: 150,
        y: 300,
        w: 220,
        h: 90,
        text: ["初始化串口", "waitForSerial"],
      },
      {
        type: "node",
        id: "runtime-setup-b",
        semantic: "primary",
        x: 420,
        y: 300,
        w: 230,
        h: 90,
        text: ["初始化本地存储", "initializeLocalStore", "loadPersistedState"],
      },
      {
        type: "node",
        id: "runtime-setup-c",
        semantic: "secondary",
        x: 700,
        y: 300,
        w: 240,
        h: 90,
        text: ["网络与模板存储", "networkExecutor.start", "initializeTemplateStore"],
      },
      {
        type: "node",
        id: "runtime-setup-d",
        semantic: "ai",
        x: 990,
        y: 300,
        w: 220,
        h: 90,
        text: ["相机与人脸引擎", "initializeCamera", "initializeFaceEngine"],
      },
      {
        type: "node",
        id: "runtime-setup-e",
        semantic: "end",
        x: 1260,
        y: 300,
        w: 220,
        h: 90,
        text: ["首轮状态更新", "wifi events / probeConnectivity", "updateView"],
      },
      {
        type: "edge",
        id: "edge-runtime-setup-a-b",
        semantic: "primary",
        from: "runtime-setup-a",
        to: "runtime-setup-b",
        fromAnchor: "right",
        toAnchor: "left",
        points: [],
      },
      {
        type: "edge",
        id: "edge-runtime-setup-b-c",
        semantic: "primary",
        from: "runtime-setup-b",
        to: "runtime-setup-c",
        fromAnchor: "right",
        toAnchor: "left",
        points: [],
      },
      {
        type: "edge",
        id: "edge-runtime-setup-c-d",
        semantic: "primary",
        from: "runtime-setup-c",
        to: "runtime-setup-d",
        fromAnchor: "right",
        toAnchor: "left",
        points: [],
      },
      {
        type: "edge",
        id: "edge-runtime-setup-d-e",
        semantic: "primary",
        from: "runtime-setup-d",
        to: "runtime-setup-e",
        fromAnchor: "right",
        toAnchor: "left",
        points: [],
      },
    ],
  },
  {
    slug: "figure-5-3",
    title: "图 5-3 设备端周期执行主循环图",
    height: 1140,
    spec: {
      main_claim:
        "设备端在 tick(nowMs) 中按固定顺序执行显示、串口配置、网络维护、相机轮询、请求派发与界面刷新，形成持续运行循环。",
      pattern: "Feedback Loop Workflow",
      secondary_pattern: "Grouped Architecture",
      reading_direction: "top-to-bottom",
    },
    shapes: [
      { type: "border", id: "border", x: 20, y: 20, w: 1614, h: 1080 },
      {
        type: "title",
        id: "title",
        x: 80,
        y: 44,
        w: 1494,
        h: 44,
        text: ["图 5-3 设备端周期执行主循环图"],
      },
      {
        type: "panel",
        id: "runtime-loop-panel",
        x: 140,
        y: 140,
        w: 1370,
        h: 860,
        label: "tick(nowMs) 周期执行阶段",
      },
      {
        type: "node",
        id: "runtime-loop-a",
        semantic: "start",
        x: 220,
        y: 250,
        w: 320,
        h: 100,
        text: ["显示与串口配置", "display.tick / processUsbProvisioning", "handleDisplayCommand"],
      },
      {
        type: "node",
        id: "runtime-loop-b",
        semantic: "primary",
        x: 610,
        y: 250,
        w: 320,
        h: 100,
        text: ["网络状态维护", "pollBootButton / wifi events", "ensureWifiConnection"],
      },
      {
        type: "node",
        id: "runtime-loop-c",
        semantic: "secondary",
        x: 1000,
        y: 250,
        w: 320,
        h: 100,
        text: [
          "请求结果消费",
          "consumeCompletedNetworkRequest",
          "probeConnectivity / probeTemplateStore",
        ],
      },
      {
        type: "node",
        id: "runtime-loop-d",
        semantic: "ai",
        x: 420,
        y: 480,
        w: 320,
        h: 110,
        text: ["相机与识别轮询", "pollCamera / face detection", "camera status refresh"],
      },
      {
        type: "node",
        id: "runtime-loop-e",
        semantic: "tertiary",
        x: 840,
        y: 480,
        w: 320,
        h: 110,
        text: ["网络请求派发", "dispatchNetworkRequest", "sync / upload / bootstrap"],
      },
      {
        type: "diamond",
        id: "runtime-loop-f",
        semantic: "decision",
        x: 610,
        y: 700,
        w: 240,
        h: 130,
        text: ["state.renderDirty", "是否需要重绘"],
      },
      {
        type: "node",
        id: "runtime-loop-g",
        semantic: "end",
        x: 910,
        y: 720,
        w: 260,
        h: 90,
        text: ["updateView", "刷新状态页面"],
      },

      {
        type: "edge",
        id: "edge-runtime-loop-a-b",
        semantic: "primary",
        from: "runtime-loop-a",
        to: "runtime-loop-b",
        fromAnchor: "right",
        toAnchor: "left",
        points: [],
      },
      {
        type: "edge",
        id: "edge-runtime-loop-b-c",
        semantic: "primary",
        from: "runtime-loop-b",
        to: "runtime-loop-c",
        fromAnchor: "right",
        toAnchor: "left",
        points: [],
      },
      {
        type: "edge",
        id: "edge-runtime-loop-b-d",
        semantic: "context",
        from: "runtime-loop-b",
        to: "runtime-loop-d",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-runtime-loop-c-e",
        semantic: "primary",
        from: "runtime-loop-c",
        to: "runtime-loop-e",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-runtime-loop-d-f",
        semantic: "primary",
        from: "runtime-loop-d",
        to: "runtime-loop-f",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-runtime-loop-e-f",
        semantic: "primary",
        from: "runtime-loop-e",
        to: "runtime-loop-f",
        fromAnchor: "left",
        toAnchor: "right",
        points: [{ x: 790, y: 760 }],
      },
      {
        type: "edge",
        id: "edge-runtime-loop-f-g",
        semantic: "primary",
        from: "runtime-loop-f",
        to: "runtime-loop-g",
        fromAnchor: "right",
        toAnchor: "left",
        points: [],
      },
      {
        type: "edge",
        id: "edge-runtime-loop-f-a",
        semantic: "feedback",
        from: "runtime-loop-f",
        to: "runtime-loop-a",
        fromAnchor: "left",
        toAnchor: "bottom",
        points: [
          { x: 260, y: 760 },
          { x: 260, y: 380 },
        ],
      },
      {
        type: "text",
        id: "label-runtime-loop-f-yes",
        x: 870,
        y: 750,
        w: 40,
        h: 24,
        text: ["是"],
        fontSize: 15,
        fill: "#7A756E",
      },
      {
        type: "text",
        id: "label-runtime-loop-f-no",
        x: 260,
        y: 740,
        w: 40,
        h: 24,
        text: ["否"],
        fontSize: 15,
        fill: "#7A756E",
      },
    ],
  },
  {
    slug: "figure-5-4",
    title: "图 5-4 设备端本地存储与同步关系图",
    height: 1080,
    spec: {
      main_claim:
        "设备端本地存储既承接启动恢复，也承接 sync 拉取、模板加载和 attendance/upload 上报，形成运行期与持久化之间的闭环。",
      pattern: "Grouped Architecture",
      secondary_pattern: "Layered Stack",
      reading_direction: "left-to-right",
    },
    shapes: [
      { type: "border", id: "border", x: 20, y: 20, w: 1614, h: 1020 },
      {
        type: "title",
        id: "title",
        x: 80,
        y: 44,
        w: 1494,
        h: 44,
        text: ["图 5-4 设备端本地存储与同步关系图"],
      },
      {
        type: "panel",
        id: "storage-rel-left",
        x: 100,
        y: 140,
        w: 430,
        h: 820,
        label: "设备运行时",
      },
      {
        type: "panel",
        id: "storage-rel-center",
        x: 570,
        y: 140,
        w: 470,
        h: 820,
        label: "本地存储层",
      },
      {
        type: "panel",
        id: "storage-rel-right",
        x: 1080,
        y: 140,
        w: 450,
        h: 820,
        label: "服务端交互",
      },

      {
        type: "node",
        id: "storage-rel-runtime",
        semantic: "ai",
        x: 150,
        y: 240,
        w: 330,
        h: 130,
        text: [
          "运行时状态",
          "deviceCode / apiKey / activationState",
          "员工快照 / 任务列表 / 队列 / 相机状态",
        ],
      },
      {
        type: "node",
        id: "storage-rel-recog",
        semantic: "primary",
        x: 150,
        y: 610,
        w: 330,
        h: 150,
        text: [
          "识别与上传逻辑",
          "本地判定 clock_in / clock_out",
          "写入待上传队列 / 清理成功记录 / 保留失败日志",
        ],
      },

      {
        type: "node",
        id: "storage-rel-nvs",
        semantic: "primary",
        x: 640,
        y: 220,
        w: 330,
        h: 100,
        text: ["NVS / Preferences", "deviceCode / apiKey / Wi-Fi / backend origin"],
      },
      {
        type: "node",
        id: "storage-rel-lfs",
        semantic: "secondary",
        x: 640,
        y: 400,
        w: 330,
        h: 170,
        text: [
          "LittleFS",
          "attendanceConfig / employees / enrollmentTasks",
          "attendance queue / failure logs",
          "template manifest / SD 健康摘要",
        ],
      },
      {
        type: "node",
        id: "storage-rel-sd",
        semantic: "tertiary",
        x: 640,
        y: 640,
        w: 330,
        h: 100,
        text: ["SD Card", "员工人脸模板库主存储"],
      },
      {
        type: "node",
        id: "storage-rel-psram",
        semantic: "ai",
        x: 640,
        y: 790,
        w: 330,
        h: 90,
        text: ["PSRAM", "运行期加载模板参与 1:N 比对"],
      },

      {
        type: "node",
        id: "storage-rel-sync",
        semantic: "secondary",
        x: 1140,
        y: 230,
        w: 330,
        h: 120,
        text: [
          "/api/device/sync",
          "返回 attendanceConfig / employees / enrollmentTasks",
          "更新 device 状态与 serverTime",
        ],
      },
      {
        type: "node",
        id: "storage-rel-hello",
        semantic: "secondary",
        x: 1140,
        y: 450,
        w: 330,
        h: 100,
        text: ["/api/device/bootstrap/hello", "签发 runtime deviceCode / apiKey"],
      },
      {
        type: "node",
        id: "storage-rel-upload",
        semantic: "secondary",
        x: 1140,
        y: 640,
        w: 330,
        h: 120,
        text: ["/api/device/attendance/upload", "批量上传本地队列", "返回逐条处理结果"],
      },

      {
        type: "edge",
        id: "edge-storage-runtime-nvs",
        semantic: "primary",
        from: "storage-rel-runtime",
        to: "storage-rel-nvs",
        fromAnchor: "right",
        toAnchor: "left",
        points: [],
      },
      {
        type: "edge",
        id: "edge-storage-runtime-lfs",
        semantic: "context",
        from: "storage-rel-runtime",
        to: "storage-rel-lfs",
        fromAnchor: "right",
        toAnchor: "left",
        points: [{ x: 550, y: 480 }],
      },
      {
        type: "edge",
        id: "edge-storage-recog-lfs",
        semantic: "primary",
        from: "storage-rel-recog",
        to: "storage-rel-lfs",
        fromAnchor: "right",
        toAnchor: "left",
        points: [{ x: 550, y: 690 }],
      },
      {
        type: "edge",
        id: "edge-storage-lfs-sd",
        semantic: "context",
        from: "storage-rel-lfs",
        to: "storage-rel-sd",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-storage-sd-psram",
        semantic: "primary",
        from: "storage-rel-sd",
        to: "storage-rel-psram",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-storage-sync-lfs",
        semantic: "context",
        from: "storage-rel-sync",
        to: "storage-rel-lfs",
        fromAnchor: "left",
        toAnchor: "right",
        points: [],
      },
      {
        type: "edge",
        id: "edge-storage-hello-nvs",
        semantic: "context",
        from: "storage-rel-hello",
        to: "storage-rel-nvs",
        fromAnchor: "left",
        toAnchor: "right",
        points: [
          { x: 1050, y: 500 },
          { x: 1050, y: 270 },
        ],
      },
      {
        type: "edge",
        id: "edge-storage-upload-lfs",
        semantic: "context",
        from: "storage-rel-upload",
        to: "storage-rel-lfs",
        fromAnchor: "left",
        toAnchor: "right",
        points: [{ x: 1050, y: 700 }],
      },
    ],
  },
  {
    slug: "figure-5-5",
    title: "图 5-5 摄像头采集与识别链路图",
    height: 1100,
    spec: {
      main_claim:
        "设备端通过 CameraPort 采集帧并更新预览，再由人脸检测与识别端口承接检测、录脸和识别结果，最终进入任务回写或考勤上传链路。",
      pattern: "Linear Workflow",
      secondary_pattern: "Grouped Architecture",
      reading_direction: "left-to-right",
    },
    shapes: [
      { type: "border", id: "border", x: 20, y: 20, w: 1614, h: 1040 },
      {
        type: "title",
        id: "title",
        x: 80,
        y: 44,
        w: 1494,
        h: 44,
        text: ["图 5-5 摄像头采集与识别链路图"],
      },
      { type: "panel", id: "camera-left", x: 100, y: 140, w: 440, h: 820, label: "采集与预览" },
      { type: "panel", id: "camera-center", x: 580, y: 140, w: 470, h: 820, label: "检测与识别" },
      {
        type: "panel",
        id: "camera-right",
        x: 1090,
        y: 140,
        w: 440,
        h: 820,
        label: "结果回写与业务处理",
      },

      {
        type: "node",
        id: "camera-a",
        semantic: "start",
        x: 150,
        y: 250,
        w: 340,
        h: 100,
        text: ["CameraPort.capture()", "采集 RGB565 帧", "相机 warmup / pollCamera"],
      },
      {
        type: "node",
        id: "camera-b",
        semantic: "secondary",
        x: 150,
        y: 450,
        w: 340,
        h: 100,
        text: ["DisplayPort.updateCameraPreview()", "把预览帧推到 LVGL 状态界面"],
      },
      {
        type: "node",
        id: "camera-c",
        semantic: "ai",
        x: 650,
        y: 240,
        w: 330,
        h: 120,
        text: ["runFaceDetection()", "HumanFaceDetect", "更新 faceDetected / faceTopScore"],
      },
      {
        type: "node",
        id: "camera-d",
        semantic: "ai",
        x: 650,
        y: 470,
        w: 330,
        h: 140,
        text: [
          "EnrollmentServicePort / RecognitionServicePort",
          "录脸任务处理",
          "本地识别得到 employeeId",
        ],
      },
      {
        type: "diamond",
        id: "camera-e",
        semantic: "decision",
        x: 690,
        y: 700,
        w: 250,
        h: 130,
        text: ["当前是录脸任务", "还是日常识别"],
      },

      {
        type: "node",
        id: "camera-f",
        semantic: "tertiary",
        x: 1140,
        y: 250,
        w: 330,
        h: 110,
        text: ["录脸任务结果回写", "/api/device/enrollment/report", "success / failed / cancelled"],
      },
      {
        type: "node",
        id: "camera-g",
        semantic: "primary",
        x: 1140,
        y: 520,
        w: 330,
        h: 130,
        text: ["考勤业务处理", "按 attendanceConfig 判定类型", "写入本地队列并等待上传"],
      },
      {
        type: "node",
        id: "camera-h",
        semantic: "end",
        x: 1140,
        y: 760,
        w: 330,
        h: 100,
        text: ["状态反馈", "更新界面、日志与后续网络请求"],
      },

      {
        type: "edge",
        id: "edge-camera-a-b",
        semantic: "primary",
        from: "camera-a",
        to: "camera-b",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-camera-a-c",
        semantic: "primary",
        from: "camera-a",
        to: "camera-c",
        fromAnchor: "right",
        toAnchor: "left",
        points: [],
      },
      {
        type: "edge",
        id: "edge-camera-b-c",
        semantic: "optional",
        from: "camera-b",
        to: "camera-c",
        fromAnchor: "right",
        toAnchor: "left",
        points: [{ x: 560, y: 500 }],
      },
      {
        type: "edge",
        id: "edge-camera-c-d",
        semantic: "primary",
        from: "camera-c",
        to: "camera-d",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-camera-d-e",
        semantic: "primary",
        from: "camera-d",
        to: "camera-e",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-camera-e-f",
        semantic: "primary",
        from: "camera-e",
        to: "camera-f",
        fromAnchor: "right",
        toAnchor: "left",
        points: [
          { x: 1070, y: 760 },
          { x: 1070, y: 305 },
        ],
      },
      {
        type: "edge",
        id: "edge-camera-e-g",
        semantic: "context",
        from: "camera-e",
        to: "camera-g",
        fromAnchor: "right",
        toAnchor: "left",
        points: [
          { x: 1070, y: 760 },
          { x: 1070, y: 585 },
        ],
      },
      {
        type: "edge",
        id: "edge-camera-f-h",
        semantic: "optional",
        from: "camera-f",
        to: "camera-h",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },
      {
        type: "edge",
        id: "edge-camera-g-h",
        semantic: "primary",
        from: "camera-g",
        to: "camera-h",
        fromAnchor: "bottom",
        toAnchor: "top",
        points: [],
      },

      {
        type: "text",
        id: "label-camera-e-task",
        x: 1080,
        y: 320,
        w: 60,
        h: 24,
        text: ["录脸"],
        fontSize: 15,
        fill: "#7A756E",
      },
      {
        type: "text",
        id: "label-camera-e-recog",
        x: 1080,
        y: 600,
        w: 60,
        h: 24,
        text: ["识别"],
        fontSize: 15,
        fill: "#7A756E",
      },
    ],
  },
];

semanticStyles.warning = {
  fill: "#F3E4DA",
  stroke: "#C88E6A",
  text: "#2D2B28",
  drawio:
    "rounded=1;whiteSpace=wrap;arcSize=10;html=1;fillColor=#F3E4DA;strokeColor=#C88E6A;strokeWidth=1.8;fontColor=#2D2B28;fontSize=18;",
};

mkdirSync(figuresDir, { recursive: true });
mkdirSync(figureSrcDir, { recursive: true });

for (const diagram of diagrams) {
  const drawio = renderDrawio(diagram);
  const svg = renderSvg(diagram);
  const html = renderHtml(diagram);
  const drawioPath = join(figureSrcDir, `${diagram.slug}.drawio`);
  const svgPath = join(figuresDir, `${diagram.slug}.svg`);
  const htmlPath = join(figureSrcDir, `${diagram.slug}.html`);

  writeFileSync(drawioPath, drawio, "utf8");
  writeFileSync(svgPath, svg, "utf8");
  writeFileSync(htmlPath, html, "utf8");
}

console.log(`Rendered ${diagrams.length} thesis figures with anthropic-diagram styling.`);
console.log(`SVG files: ${figuresDir}`);
console.log(`Drawio files: ${figureSrcDir}`);

function renderDrawio(diagram) {
  const body = [];
  body.push('<mxCell id="0"/>');
  body.push('<mxCell id="1" parent="0"/>');

  for (const shape of diagram.shapes) {
    if (shape.type === "border") {
      body.push(
        vertexCell(
          shape.id,
          "",
          "rounded=0;arcSize=3;fillColor=none;strokeColor=#B9B3AB;strokeWidth=1.5;pointerEvents=0;",
          shape.x,
          shape.y,
          shape.w,
          shape.h,
        ),
      );
      continue;
    }

    if (shape.type === "title") {
      body.push(
        textCell(
          shape.id,
          shape.text,
          shape.x,
          shape.y,
          shape.w,
          shape.h,
          32,
          "#1F1F1C",
          "center",
          true,
        ),
      );
      continue;
    }

    if (shape.type === "text") {
      body.push(
        textCell(
          shape.id,
          shape.text,
          shape.x,
          shape.y,
          shape.w,
          shape.h,
          shape.fontSize ?? 16,
          shape.fill ?? "#4F4A44",
          "center",
          false,
        ),
      );
      continue;
    }

    if (shape.type === "panel") {
      body.push(
        vertexCell(
          shape.id,
          `&lt;font style=&quot;font-size: 22px;&quot;&gt;${escapeXml(shape.label)}&lt;/font&gt;`,
          panelStyle,
          shape.x,
          shape.y,
          shape.w,
          shape.h,
        ),
      );
      continue;
    }

    if (shape.type === "entity") {
      const style = entityStyles[shape.semantic] ?? entityStyles.primary;
      body.push(
        vertexCell(
          shape.id,
          `&lt;font style=&quot;font-size: 20px;&quot;&gt;${escapeXml(shape.title)}&lt;/font&gt;`,
          style,
          shape.x,
          shape.y,
          shape.w,
          shape.h,
        ),
      );
      body.push(
        textCell(
          `${shape.id}-body`,
          shape.note ? [...shape.fields, shape.note] : shape.fields,
          shape.x + 20,
          shape.y + 56,
          shape.w - 40,
          shape.h - 66,
          16,
          shape.semantic === "inactive" ? "#7A756E" : "#2D2B28",
          "left",
          false,
        ),
      );
      continue;
    }

    if (shape.type === "node") {
      const style = semanticStyles[shape.semantic]?.drawio ?? semanticStyles.primary.drawio;
      body.push(
        vertexCell(shape.id, htmlLines(shape.text), style, shape.x, shape.y, shape.w, shape.h),
      );
      continue;
    }

    if (shape.type === "diamond") {
      body.push(
        vertexCell(
          shape.id,
          htmlLines(shape.text),
          semanticStyles.decision.drawio,
          shape.x,
          shape.y,
          shape.w,
          shape.h,
        ),
      );
      continue;
    }

    if (shape.type === "cylinder") {
      const tone = semanticStyles[shape.semantic] ?? semanticStyles.secondary;
      const cylinderStyle = `shape=cylinder3;whiteSpace=wrap;boundedLbl=1;backgroundOutline=1;size=15;html=1;fillColor=${tone.fill};strokeColor=${tone.stroke};strokeWidth=1.8;fontColor=${tone.text};fontSize=18;`;
      body.push(
        vertexCell(
          shape.id,
          htmlLines(shape.text),
          cylinderStyle,
          shape.x,
          shape.y,
          shape.w,
          shape.h,
        ),
      );
      continue;
    }

    if (shape.type === "edge") {
      body.push(edgeCell(shape));
    }
  }

  return `<?xml version="1.0" encoding="UTF-8"?>
<mxfile host="app.diagrams.net" modified="${new Date().toISOString()}" agent="Codex" version="24.7.17" compressed="false">
  <diagram id="${diagram.slug}" name="Page-1">
    <mxGraphModel background="${canvas.background}" grid="0" tooltips="0" connect="0" arrows="0" fold="0" page="0" pageScale="1" pageWidth="${canvas.width}" pageHeight="${diagram.height}" math="0" shadow="0">
      <root>
        ${body.join("\n        ")}
      </root>
    </mxGraphModel>
  </diagram>
</mxfile>
`;
}

function renderHtml(diagram) {
  return `<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>${escapeXml(diagram.title)}</title>
  <style>
    body {
      margin: 0;
      padding: 24px;
      background: #ffffff;
    }
    .figure-wrap {
      width: fit-content;
      margin: 0 auto;
      background: #ffffff;
    }
    img {
      display: block;
      max-width: min(100vw - 48px, 1400px);
      height: auto;
      background: #ffffff;
    }
  </style>
</head>
<body>
  <div class="figure-wrap">
    <img src="../figures/${diagram.slug}.svg" alt="${escapeXml(diagram.title)}" />
  </div>
</body>
</html>
`;
}

function renderSvg(diagram) {
  const parts = [];
  parts.push(
    `<?xml version="1.0" encoding="UTF-8"?>`,
    `<svg xmlns="http://www.w3.org/2000/svg" width="${canvas.width}" height="${diagram.height}" viewBox="0 0 ${canvas.width} ${diagram.height}" role="img" aria-label="${escapeXml(
      diagram.title,
    )}">`,
    `<defs>`,
    markerDef("arrow-primary", "#7A756E"),
    markerDef("arrow-optional", "#9A948C"),
    markerDef("arrow-feedback", "#8E8982"),
    markerDef("arrow-context", "#7FB08F"),
    erMarkerDef("er-one", "one", "#7A756E"),
    erMarkerDef("er-many", "many", "#7A756E"),
    erMarkerDef("er-zero-to-one", "zeroToOne", "#7A756E"),
    erMarkerDef("er-zero-to-many", "zeroToMany", "#7A756E"),
    `</defs>`,
    `<rect x="0" y="0" width="${canvas.width}" height="${diagram.height}" fill="${canvas.background}"/>`,
  );

  for (const shape of diagram.shapes) {
    if (shape.type === "border") {
      parts.push(
        `<rect x="${shape.x}" y="${shape.y}" width="${shape.w}" height="${shape.h}" rx="4" ry="4" fill="none" stroke="#B9B3AB" stroke-width="1.5"/>`,
      );
      continue;
    }

    if (shape.type === "title") {
      parts.push(
        svgText(shape.x, shape.y, shape.w, shape.h, shape.text, {
          fontSize: 32,
          fill: "#1F1F1C",
          fontWeight: 700,
        }),
      );
      continue;
    }

    if (shape.type === "text") {
      parts.push(
        svgText(shape.x, shape.y, shape.w, shape.h, shape.text, {
          fontSize: shape.fontSize ?? 16,
          fill: shape.fill ?? "#4F4A44",
        }),
      );
      continue;
    }

    if (shape.type === "panel") {
      parts.push(svgPanel(shape));
      continue;
    }

    if (shape.type === "entity") {
      parts.push(svgEntity(shape));
      continue;
    }

    if (shape.type === "node") {
      parts.push(svgNode(shape));
      continue;
    }

    if (shape.type === "diamond") {
      parts.push(svgDiamond(shape));
      continue;
    }

    if (shape.type === "cylinder") {
      parts.push(svgCylinder(shape));
      continue;
    }
  }

  for (const shape of diagram.shapes.filter((shape) => shape.type === "edge")) {
    parts.push(svgEdge(shape, diagram));
  }

  parts.push(`</svg>`);
  return parts.join("\n");
}

function svgNode(shape) {
  const tone = semanticStyles[shape.semantic] ?? semanticStyles.primary;
  const radius = shape.semantic === "start" ? 30 : 14;
  return [
    `<rect x="${shape.x}" y="${shape.y}" width="${shape.w}" height="${shape.h}" rx="${radius}" ry="${radius}" fill="${tone.fill}" stroke="${tone.stroke}" stroke-width="1.8"/>`,
    svgText(shape.x, shape.y, shape.w, shape.h, shape.text, {
      fontSize: 18,
      fill: tone.text,
      fontWeight: shape.semantic === "start" ? 700 : 500,
    }),
  ].join("\n");
}

function svgPanel(shape) {
  const headerHeight = 54;
  return [
    `<rect x="${shape.x}" y="${shape.y}" width="${shape.w}" height="${shape.h}" rx="20" ry="20" fill="#FAF8F4" stroke="#8C867F" stroke-width="2"/>`,
    `<path d="M ${shape.x + 20} ${shape.y + headerHeight} L ${shape.x + shape.w - 20} ${shape.y + headerHeight}" fill="none" stroke="#D4CEC5" stroke-width="1.3"/>`,
    svgText(shape.x + 28, shape.y + 12, shape.w - 56, 30, [shape.label], {
      fontSize: 22,
      fill: "#5F5A54",
      fontWeight: 700,
      anchor: "start",
      alignX: shape.x + 28,
    }),
  ].join("\n");
}

function svgEntity(shape) {
  const tone =
    shape.semantic === "secondary"
      ? { fill: "#F7FBFF", stroke: "#6FA8D6", text: "#2D2B28" }
      : shape.semantic === "tertiary"
        ? { fill: "#F7F5FE", stroke: "#9A90D6", text: "#2D2B28" }
        : shape.semantic === "inactive"
          ? { fill: "#FAF8F4", stroke: "#B4AEA6", text: "#7A756E" }
          : { fill: "#FAF8F4", stroke: "#8C867F", text: "#2D2B28" };
  const headerHeight = 48;
  const lines = shape.note ? [...shape.fields, shape.note] : shape.fields;

  return [
    `<rect x="${shape.x}" y="${shape.y}" width="${shape.w}" height="${shape.h}" rx="14" ry="14" fill="${tone.fill}" stroke="${tone.stroke}" stroke-width="1.8"/>`,
    `<path d="M ${shape.x + 12} ${shape.y + headerHeight} L ${shape.x + shape.w - 12} ${shape.y + headerHeight}" fill="none" stroke="${tone.stroke}" stroke-width="1.2"/>`,
    svgText(shape.x, shape.y + 8, shape.w, 28, [shape.title], {
      fontSize: 20,
      fill: tone.text,
      fontWeight: 700,
    }),
    svgText(shape.x + 18, shape.y + 58, shape.w - 36, shape.h - 72, lines, {
      fontSize: 16,
      fill: tone.text,
      anchor: "start",
      alignX: shape.x + 24,
      lineHeight: 22,
    }),
  ].join("\n");
}

function svgDiamond(shape) {
  const tone = semanticStyles.decision;
  const x1 = shape.x + shape.w / 2;
  const y1 = shape.y;
  const x2 = shape.x + shape.w;
  const y2 = shape.y + shape.h / 2;
  const x3 = shape.x + shape.w / 2;
  const y3 = shape.y + shape.h;
  const x4 = shape.x;
  const y4 = shape.y + shape.h / 2;

  return [
    `<polygon points="${x1},${y1} ${x2},${y2} ${x3},${y3} ${x4},${y4}" fill="${tone.fill}" stroke="${tone.stroke}" stroke-width="1.8"/>`,
    svgText(shape.x + 20, shape.y + 18, shape.w - 40, shape.h - 36, shape.text, {
      fontSize: 17,
      fill: tone.text,
    }),
  ].join("\n");
}

function svgCylinder(shape) {
  const tone = semanticStyles[shape.semantic] ?? semanticStyles.secondary;
  const rx = shape.w / 2;
  const ry = 16;
  const cx = shape.x + shape.w / 2;
  const topY = shape.y + ry;
  const bottomY = shape.y + shape.h - ry;

  return [
    `<path d="M ${shape.x} ${topY} C ${shape.x} ${shape.y + 6}, ${shape.x + shape.w} ${shape.y + 6}, ${shape.x + shape.w} ${topY} L ${shape.x + shape.w} ${bottomY} C ${shape.x + shape.w} ${shape.y + shape.h + 8}, ${shape.x} ${shape.y + shape.h + 8}, ${shape.x} ${bottomY} Z" fill="${tone.fill}" stroke="${tone.stroke}" stroke-width="1.8"/>`,
    `<ellipse cx="${cx}" cy="${topY}" rx="${rx}" ry="${ry}" fill="${tone.fill}" stroke="${tone.stroke}" stroke-width="1.8"/>`,
    `<path d="M ${shape.x} ${bottomY} C ${shape.x} ${shape.y + shape.h + 8}, ${shape.x + shape.w} ${shape.y + shape.h + 8}, ${shape.x + shape.w} ${bottomY}" fill="none" stroke="${tone.stroke}" stroke-width="1.8"/>`,
    svgText(shape.x, shape.y + 22, shape.w, shape.h - 18, shape.text, {
      fontSize: 18,
      fill: tone.text,
    }),
  ].join("\n");
}

function svgEdge(shape, diagram) {
  const tone = edgeStyles[shape.semantic] ?? edgeStyles.primary;
  const from = getConnectorPoint(diagram, shape.from, shape.fromAnchor ?? "right");
  const to = getConnectorPoint(diagram, shape.to, shape.toAnchor ?? "left");
  const points = [from, ...(shape.points ?? []), to];
  const marker =
    shape.semantic === "optional"
      ? "arrow-optional"
      : shape.semantic === "feedback"
        ? "arrow-feedback"
        : shape.semantic === "context"
          ? "arrow-context"
          : "arrow-primary";

  const startMarker = erMarkerRef(shape.startArrow);
  const endMarker = erMarkerRef(shape.endArrow) ?? `url(#${marker})`;

  return `<polyline points="${points.map((point) => `${point.x},${point.y}`).join(" ")}" fill="none" stroke="${tone.stroke}" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"${
    tone.dasharray ? ` stroke-dasharray="${tone.dasharray}"` : ""
  }${startMarker ? ` marker-start="${startMarker}"` : ""} marker-end="${endMarker}"/>`;
}

function getConnectorPoint(diagram, id, mode) {
  const shape = diagram.shapes.find((item) => item.id === id);
  if (!shape) {
    throw new Error(`Unknown shape id: ${id}`);
  }

  switch (mode) {
    case "left":
      return { x: shape.x, y: shape.y + shape.h / 2 };
    case "right":
      return { x: shape.x + shape.w, y: shape.y + shape.h / 2 };
    case "top":
      return { x: shape.x + shape.w / 2, y: shape.y };
    case "bottom":
      return { x: shape.x + shape.w / 2, y: shape.y + shape.h };
    case "center":
      return { x: shape.x + shape.w / 2, y: shape.y + shape.h / 2 };
    default:
      return { x: shape.x + shape.w, y: shape.y + shape.h / 2 };
  }
}

function markerDef(id, stroke) {
  return `<marker id="${id}" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse">
  <path d="M 0 0 L 10 5 L 0 10" fill="none" stroke="${stroke}" stroke-width="1.4"/>
</marker>`;
}

function erMarkerDef(id, kind, stroke) {
  const common = `<marker id="${id}" viewBox="0 0 20 20" refX="18" refY="10" markerWidth="14" markerHeight="14" orient="auto-start-reverse">`;

  if (kind === "one") {
    return `${common}
  <path d="M 14 2 L 14 18" fill="none" stroke="${stroke}" stroke-width="1.5"/>
</marker>`;
  }

  if (kind === "many") {
    return `${common}
  <path d="M 4 10 L 16 4" fill="none" stroke="${stroke}" stroke-width="1.5"/>
  <path d="M 4 10 L 16 10" fill="none" stroke="${stroke}" stroke-width="1.5"/>
  <path d="M 4 10 L 16 16" fill="none" stroke="${stroke}" stroke-width="1.5"/>
</marker>`;
  }

  if (kind === "zeroToOne") {
    return `${common}
  <circle cx="8" cy="10" r="3" fill="none" stroke="${stroke}" stroke-width="1.4"/>
  <path d="M 16 2 L 16 18" fill="none" stroke="${stroke}" stroke-width="1.5"/>
</marker>`;
  }

  return `${common}
  <circle cx="6" cy="10" r="3" fill="none" stroke="${stroke}" stroke-width="1.4"/>
  <path d="M 8 10 L 18 4" fill="none" stroke="${stroke}" stroke-width="1.5"/>
  <path d="M 8 10 L 18 10" fill="none" stroke="${stroke}" stroke-width="1.5"/>
  <path d="M 8 10 L 18 16" fill="none" stroke="${stroke}" stroke-width="1.5"/>
</marker>`;
}

function erMarkerRef(marker) {
  switch (marker) {
    case "ERone":
      return "url(#er-one)";
    case "ERmany":
      return "url(#er-many)";
    case "ERzeroToOne":
      return "url(#er-zero-to-one)";
    case "ERzeroToMany":
      return "url(#er-zero-to-many)";
    default:
      return null;
  }
}

function htmlLines(lines) {
  return lines.map((line) => escapeXml(line)).join("&lt;br/&gt;");
}

function textCell(id, lines, x, y, w, h, fontSize, color, align, bold) {
  const value = htmlLines(lines);
  const alignPart = align === "left" ? "align=left;" : "align=center;";
  const fontStyle = bold ? "fontStyle=1;" : "";
  return `<mxCell id="${id}" value="${value}" style="text;html=1;strokeColor=none;fillColor=none;${alignPart}verticalAlign=middle;whiteSpace=wrap;overflow=hidden;${fontStyle}fontSize=${fontSize};fontColor=${color};" vertex="1" parent="1">
  <mxGeometry x="${x}" y="${y}" width="${w}" height="${h}" as="geometry"/>
</mxCell>`;
}

function vertexCell(id, value, style, x, y, w, h) {
  return `<mxCell id="${id}" value="${value}" style="${style}" vertex="1" parent="1">
  <mxGeometry x="${x}" y="${y}" width="${w}" height="${h}" as="geometry"/>
</mxCell>`;
}

function edgeCell(shape) {
  const baseStyle = edgeStyles[shape.semantic]?.drawio ?? edgeStyles.primary.drawio;
  const style = `${shape.drawioEdgeStyle ? baseStyle.replace("edgeStyle=orthogonalEdgeStyle;", `edgeStyle=${shape.drawioEdgeStyle};`) : baseStyle}${anchorStyle(
    shape.fromAnchor,
    "exit",
  )}${anchorStyle(shape.toAnchor, "entry")}${shape.startArrow ? `startArrow=${shape.startArrow};startFill=0;` : ""}${
    shape.endArrow ? `endArrow=${shape.endArrow};endFill=0;` : ""
  }`;
  const points = shape.points?.length
    ? `
    <Array as="points">
${shape.points.map((point) => `      <mxPoint x="${point.x}" y="${point.y}"/>`).join("\n")}
    </Array>`
    : "";

  return `<mxCell id="${shape.id}" edge="1" source="${shape.from}" target="${shape.to}" style="${style}" parent="1">
  <mxGeometry relative="1" as="geometry">${points}
  </mxGeometry>
</mxCell>`;
}

function anchorStyle(anchor, prefix) {
  switch (anchor) {
    case "left":
      return `${prefix}X=0;${prefix}Y=0.5;`;
    case "right":
      return `${prefix}X=1;${prefix}Y=0.5;`;
    case "top":
      return `${prefix}X=0.5;${prefix}Y=0;`;
    case "bottom":
      return `${prefix}X=0.5;${prefix}Y=1;`;
    default:
      return "";
  }
}

function svgText(x, y, w, h, lines, options = {}) {
  const fontSize = options.fontSize ?? 18;
  const lineHeight = options.lineHeight ?? Math.round(fontSize * 1.32);
  const fill = options.fill ?? "#2D2B28";
  const fontWeight = options.fontWeight ?? 500;
  const anchor = options.anchor ?? "middle";
  const anchorX = options.alignX ?? x + w / 2;
  const lineCount = lines.length;
  const totalHeight = lineHeight * (lineCount - 1);
  const startY = y + h / 2 - totalHeight / 2;
  const fontFamily = "Helvetica, Arial, sans-serif";

  return `<text x="${anchorX}" y="${startY}" text-anchor="${anchor}" dominant-baseline="middle" fill="${fill}" font-size="${fontSize}" font-weight="${fontWeight}" font-family="${fontFamily}">${lines
    .map(
      (line, index) =>
        `<tspan x="${anchorX}" dy="${index === 0 ? 0 : lineHeight}">${escapeXml(line)}</tspan>`,
    )
    .join("")}</text>`;
}

function escapeXml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&apos;");
}
