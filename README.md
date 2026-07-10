# Sleep Recovery

一个面向 `macOS` 的睡眠恢复评估项目原型。

项目目标是基于公开可穿戴睡眠数据（Fitbit），构建一条完整的工程链路：

- 使用 `C++` 做数据适配、清洗、标准化与特征提取
- 使用 `Python / Colab` 做模型训练、对比与 `Core ML` 导出验证
- 使用 `SwiftUI + Core ML + Charts` 在 `macOS` 端做本地推理与图表呈现

当前阶段以 `MVP` 为主，重点不是做医学诊断，而是验证一套真实可落地的端到端数据与模型部署流程。

## 项目聚焦

MVP 场景为 `睡眠恢复评估`：

- 输入：Fitbit 睡眠数据中的夜间心率、活动、睡眠摘要等字段
- 处理：统一 schema、弱标签生成、特征包生成、模型训练与 Core ML 导出
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
- 主部署模型：`Logistic Regression`（BalancedAcc=0.69, F1=0.736）
- 回退模型：`Random Forest`
- 中期候选：`XGBoost`

## 当前状态

**MVP 开发已完成。**

完成内容：
- ✅ M1: 数据契约锁定 + 黄金样本（用户 2347167796, 4/1/2016, label=Good）
- ✅ M2: C++ 预处理 CLI + 240 个会话特征包产出
- ✅ M3: 3 模型训练（LR/RF/XGB）+ Core ML 导出验证
- ✅ M4: macOS SwiftUI 应用 + 全链路一致性验证

全链路验证结果：
- C++ ↔ Python: 9 特征 diff=0.0 ✓
- Python ↔ Core ML: LR=0.817, RF=0.979, XGB=0.996 ✓
- C++ ↔ Core ML: 黄金样本一致 ✓
- swift build: 编译+启动通过 ✓

## 关键文档

- 统一 Spec & Milestone：[spec-and-milestones.md](docs/spec-and-milestones.md)
- 数据契约：[data-contract.md](docs/data-contract.md)
- MVP Spec：[spec.md](.scratch/sleep-recovery-mvp/spec.md)
- ADR：[0001-mvp-data-and-deployment-shape.md](docs/adr/0001-mvp-data-and-deployment-shape.md)
- 一致性验证：[consistency-report.md](docs/consistency-report.md)
- 模型选择分析：[model-selection-analysis.md](docs/model-selection-analysis.md)
- Core ML 优化：[coreml-optimization.md](docs/coreml-optimization.md)
- 数据集选择：[data_readme.md](data_readme.md)
- 演示说明：[DEMO.md](DEMO.md)
- Tickets 目录：[issues](.scratch/sleep-recovery-mvp/issues)
- Milestone Notes：[milestones](.scratch/sleep-recovery-mvp/milestones)
- 项目上下文：[CONTEXT.md](CONTEXT.md)

## MVP 里程碑

| 里程碑 | 状态 | 产出 |
|---|---|---|
| M1: Data Foundation | ✅ | 数据契约、黄金样本、目录结构 |
| M2: Preprocessing | ✅ | C++ CLI、240 sessions、feature package |
| M3: Training & Adapter | ✅ | 3 模型训练、Core ML 导出 |
| M4: macOS & Delivery | ✅ | SwiftUI 应用、全链路验证 |

## 目录概览

```text
.
├── cpp/                    # C++ 预处理 CLI
│   ├── Makefile
│   └── src/                # 11 个源文件
├── python/                 # Python 训练脚本
│   ├── train_models.py     # 训练 + 评估 + Core ML 导出
│   └── extract_golden_sample.py
├── models/                 # Core ML 模型
│   ├── lr_classifier.mlpackage
│   ├── rf_classifier.mlpackage
│   └── xgb_classifier_v2.mlpackage
├── swift-app/              # SwiftUI macOS 应用
│   └── SleepRecovery/
├── data/                   # 数据产出
│   ├── features/           # 240 个会话特征包
│   ├── normalized/         # 标准化会话数据
│   ├── labels/             # 弱标签
│   └── samples/golden/     # 黄金样本
├── docs/                   # 文档
│   ├── spec-and-milestones.md
│   ├── data-contract.md
│   ├── consistency-report.md
│   ├── model-selection-analysis.md
│   ├── coreml-optimization.md
│   └── adr/
├── .scratch/               # 本地 issue tracker
│   └── sleep-recovery-mvp/
│       ├── spec.md
│       ├── issues/         # 7 tickets
│       └── milestones/     # 4 milestone notes
├── AGENTS.md
├── CLAUDE.md
├── CONTEXT.md
├── DEMO.md
└── README.md
```

## 项目边界

当前 `MVP` 明确不包含：

- 医学诊断或临床级结论
- 实时原始流数据端上特征提取
- iOS 部署
- Apple Watch 等真实硬件接入

## 快速开始

```bash
# C++ CLI
cd cpp && make
./build/sleep-recovery-cli --input "Fitabase Data 3.12.16-4.11.16/" ...

# Python 训练
cd python && uv run python train_models.py

# SwiftUI 应用
cd swift-app && swift build
```

详见 [DEMO.md](DEMO.md)
