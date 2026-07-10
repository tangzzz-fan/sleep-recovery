# Sleep Recovery

一个面向 `macOS` 的睡眠恢复评估项目原型。

项目目标是基于公开可穿戴睡眠数据，构建一条完整的工程链路：

- 使用 `C++` 做数据适配、清洗、标准化与特征提取
- 使用 `Python / Colab` 做模型训练、对比与 `Core ML` 导出验证
- 使用 `SwiftUI + Core ML + Charts` 在 `macOS` 端做本地推理与图表呈现

当前阶段以 `MVP` 为主，重点不是做医学诊断，而是验证一套真实可落地的端到端数据与模型部署流程。

## 项目聚焦

MVP 场景为 `睡眠恢复评估`：

- 输入：公开睡眠数据中的夜间心率、活动、睡眠摘要等字段
- 处理：统一 schema、弱标签生成、特征包生成、模型训练与导出
- 输出：恢复等级预测、概率信息、关键影响因素、单夜与多夜趋势图

## 技术路线

项目采用四层结构：

1. `C++`
   负责数据适配、预处理、标准化时序和 feature package 生成
2. `Python / Colab`
   负责模型训练、模型比较、模型适配层和 Core ML 导出验证
3. `Core ML`
   作为训练结果与 macOS 集成之间的独立验证 seam
4. `SwiftUI`
   负责 macOS 本地加载、推理结果展示和图表渲染

当前主路线：

- 主数据集：`Bellabeat / Fitbit Fitness Tracker Data`
- 标签策略：`规则弱标签`
- 主模型候选：`XGBoost / LightGBM`
- 回退模型：`Logistic Regression`、`Random Forest`

## 当前状态

当前仓库已经完成：

- 项目场景选择与范围收敛
- MVP 数据方案与阶段计划文档
- ADR 架构决策记录
- 基于 spec 生成的本地 tickets
- 数据集选择说明与主推荐数据集更新

当前尚未开始正式实现：

- 数据契约锁定与黄金样本
- 单夜预处理管道
- 基线训练与模型适配层
- Core ML 导出验证
- macOS 分析页实现

## 关键文档

- 项目方案：[sleep-recovery-project-plan.md](docs/sleep-recovery-project-plan.md)
- 数据集选择：[data_readme.md](data_readme.md)
- MVP Spec：[spec.md](.scratch/sleep-recovery-mvp/spec.md)
- ADR：[0001-mvp-data-and-deployment-shape.md](docs/adr/0001-mvp-data-and-deployment-shape.md)
- Tickets 目录：[issues](.scratch/sleep-recovery-mvp/issues)
- 项目上下文：[CONTEXT.md](CONTEXT.md)

## MVP 里程碑

当前 MVP 拆分为 7 个交付节点：

1. 锁定数据契约与黄金样本
2. 打通单夜预处理到特征包
3. 建立基线训练与模型适配层
4. 验证 Core ML 主线与回退路径
5. 交付单夜 macOS 分析页
6. 交付多夜趋势视图
7. 收口 MVP 文档与验证闭环

## 目录概览

当前仓库以文档与规划产物为主：

```text
.
├─ .scratch/
│  └─ sleep-recovery-mvp/
│     ├─ spec.md
│     └─ issues/
├─ docs/
│  ├─ adr/
│  └─ agents/
├─ AGENTS.md
├─ CONTEXT.md
├─ data_readme.md
└─ README.md
```

后续实现阶段会逐步补齐：

- `cpp/`
- `python/`
- `models/`
- `swift-app/`
- `data/`

## 项目边界

当前 `MVP` 明确不包含：

- 医学诊断或临床级结论
- 实时原始流数据端上特征提取
- iOS 部署
- Apple Watch 等真实硬件接入

## 下一步

建议从第一个 ticket 开始推进：

- [01-lock-data-contract-and-golden-sample.md](.scratch/sleep-recovery-mvp/issues/01-lock-data-contract-and-golden-sample.md)

完成这一阶段后，后续的 `C++` 预处理、训练、Core ML 导出和 Swift 展示都会有稳定的输入契约可依赖。
