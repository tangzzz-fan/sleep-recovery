# 07 — 收口 MVP 文档与验证闭环

**What to build:** 固化整个 sleep recovery MVP 的验证、模型取舍、Core ML 优化分析和演示说明，形成可交付、可复现、可继续拆解实现的最终交付包。

**Blocked by:** 04 — 验证 Core ML 主线与回退路径; 05 — 交付单夜 macOS 分析页; 06 — 交付多夜趋势视图

**Status:** ready-for-agent

- [ ] 输出跨 C++、Python、Core ML、Swift 的一致性验证结论
- [ ] 输出模型主线与回退路径的工程权衡说明
- [ ] 输出 Core ML 优化分析与最终部署建议
- [ ] 输出 MVP 演示说明，使其他人能理解当前闭环范围和限制
- [ ] 当前 `.scratch/sleep-recovery-mvp/` 下的 spec 与 tickets 保持一致，不出现范围漂移
