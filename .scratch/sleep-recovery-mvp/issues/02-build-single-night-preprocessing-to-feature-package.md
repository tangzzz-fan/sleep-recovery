# 02 — 打通单夜预处理到特征包

**What to build:** 从一晚原始或适配后的睡眠数据稳定产出标准化心率/活动时序和 feature package，让训练侧与 Swift 展示侧使用同一份可复用输入。

**Blocked by:** 01 — 锁定数据契约与黄金样本

**Status:** ready-for-agent

- [ ] 实现 Kaggle 数据适配到统一 schema 的最小流程
- [ ] 实现单夜标准化时序输出，至少覆盖心率和活动数据
- [ ] 实现 MVP 所需的核心特征提取，并输出可训练的 feature package
- [ ] 让黄金样本能从原始输入稳定生成同结构输出
- [ ] 输出结果可同时被 Python 训练侧和 Swift 展示侧读取
