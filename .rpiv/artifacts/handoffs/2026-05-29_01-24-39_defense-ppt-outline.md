---
date: 2026-05-29T01:24:39+0800
author: JinJieBeWater
commit: 4479261
branch: main
repository: hitomi
topic: "答辩 PPT 大纲设计"
tags: [defense, ppt, outline, discover-interview]
status: complete
last_updated: 2026-05-29T01:24:39+0800
last_updated_by: JinJieBeWater
type: implementation_strategy
---

# Handoff: 答辩 PPT 大纲设计完成，待截图 + 做幻灯片

## Task(s)

1. **PPT 大纲设计** — 已完成。产出物：`docs/defense/ppt-outline.md`，6 页结构定稿。
2. **discover 结构化访谈** — 已完成。通过 5 轮问答确认了核心目标、配图策略、页面顺序、内容密度、P6 收尾方式。

## Critical References

- `docs/defense/ppt-outline.md` — 最终大纲（含隐线检查、口述要点、待截图清单）
- `docs/defense-demo/demo-runbook.md` — 现场演示验证流程（11 步，含备用策略）
- `docs/defense-demo/plan.md` — 答辩准备总体计划

## Recent changes

- `docs/defense/ppt-outline.md` — 新建并多轮迭代：架构图标注协议、按故事线重排 P3-P5、精简密度、统一学术答辩用词
- `docs/defense/` 目录 — 本次会话创建

## Learnings

- 教师要求：6 页 PPT，背景只放一页不口述，重点演示功能。答辩策略为"PPT 建立上下文 + 现场演示证明系统能跑"。
- 两套隐线贯穿全 PPT：「物联网工程向」（集成非自研、多领域聚合）和「企业级 B2B」（组织内部部署、非 to C）。落点已写入每页末端检查表。
- PPT 页面顺序从"层视角"改为"故事视角"：核心功能（打卡怎么产生的）→ 首次配置（一次性前提）→ 日常使用（管理员每天看到的）。以首次配置/日常使用为分界点。
- 内容密度控制：每页 3~4 行正文 + 2~3 张截图。口述每页 1~2 句。

## Artifacts

- `docs/defense/ppt-outline.md` — 6 页 PPT 大纲（含隐线检查、口述要点、待截图清单，7 张图）

## Action Items & Next Steps

1. **跑演示验证**：按 `docs/defense-demo/demo-runbook.md` 走一遍主链路，确认考勤闭环稳定。
2. **截图**：按 `docs/defense/ppt-outline.md` 末尾清单截 7 张图（P3: 设备实拍+屏幕、P4: 员工列表+考勤配置+设备列表、P5: 考勤记录+仪表盘）。
3. **做 PPT 幻灯片**：用加图后的大纲生成幻灯片。可选方案：(a) `image2-ppt-skill` 自动生成，(b) Keynote/PowerPoint 手动制作。
4. **计时演练**：按 2 分钟 PPT + 5~8 分钟演示完成一次完整模拟。

## Other Notes

- `output/playwright/` 下有旧截图（`employees-muted-colors.png`、`sidebar-collapsed.png`），UI 有改动需重新截。
- `docs/figures/` 有 14 张论文配图（PNG+SVG），可供 PPT 参考但建议优先用截图+实拍。
- 技术栈表删除了 Playwright（答辩不涉及），保留了 Turborepo + PlatformIO 体现工程工具链。
- 回避用词清单：算法、模型、训练、精度、用户、App。人脸识别全程表述为"集成 ESP-WHO 方案"。
