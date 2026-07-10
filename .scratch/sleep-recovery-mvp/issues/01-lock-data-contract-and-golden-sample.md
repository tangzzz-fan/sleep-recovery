# 01 — 锁定数据契约与黄金样本

**What to build:** 选定 1 个可进入 MVP 的 Kaggle 可穿戴睡眠 CSV 数据集，并把它映射到当前项目的统一 sleep recovery schema；同时锁定弱标签规则、artifact manifest 和黄金样本，使后续训练、Core ML 和 Swift 展示都围绕同一数据契约工作。

**Blocked by:** None — can start immediately.

**Status:** ready-for-agent

- [ ] 选定 1 个 Kaggle 主数据集，并记录为什么它适合当前 MVP
- [ ] 明确原始字段到 `night session`、标准化时序和特征包字段的映射规则
- [ ] 锁定 `recovery_label` 的弱标签生成规则，避免后续训练阶段再次改口径
- [ ] 产出一份 `artifact manifest`，包含 schema version、列顺序、标签映射和数据来源标记
- [ ] 产出 1 份黄金样本定义，至少包含标准化时序、特征包和期望预测结果占位
