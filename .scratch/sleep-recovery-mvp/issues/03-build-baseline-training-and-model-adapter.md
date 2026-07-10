# 03 — 建立基线训练与模型适配层

**What to build:** 使用同一份 feature package 训练并比较 Logistic Regression、Random Forest 和 XGBoost/LightGBM，同时建立统一的模型适配层，让不同模型共享相同的特征顺序、标签映射和元数据契约。

**Blocked by:** 02 — 打通单夜预处理到特征包

**Status:** ready-for-agent

- [ ] 基于统一 feature package 跑通至少 3 类候选分类模型
- [ ] 输出可重复的训练评估结果，包括分类效果和主要特征贡献信息
- [ ] 建立模型适配层，统一特征顺序、标签映射和模型元数据
- [ ] 明确主部署候选模型与回退候选模型
- [ ] 记录模型取舍理由，体现效果、解释性和部署风险的权衡
