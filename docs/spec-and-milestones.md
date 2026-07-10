# Sleep Recovery MVP — Spec & Milestones

> **本文档是 MVP 阶段的权威参考。** 综合自 `spec.md`、`project-plan.md`、`ADR`、`milestone-plan.md` 和 7 个 tickets。
> **与其他文档冲突时，以本文档为准。** 详见 [Section 8 文档治理](#8-文档治理)。

---

## 1. Project Overview

**Sleep Recovery** 是一个 macOS 睡眠恢复评估原型。目标是在没有自有硬件的前提下，基于 Kaggle 公开可穿戴睡眠数据构建一条完整的工程链路：

```
Kaggle 公开数据 → C++ 预处理 → Python/Colab 训练 → Core ML 导出 → SwiftUI macOS 展示
```

**核心场景**：输入夜间心率、活动、睡眠摘要等字段，输出恢复等级（差/一般/良好）、概率、关键影响因素和多夜趋势图。

**数据策略**：以 Kaggle Bellabeat/Fitbit 公开数据为主训练数据，合成数据仅作为字段补齐和兜底。详见 [Section 2.1](#21-data-architecture)。

**这不是医疗诊断工具**，而是一个端到端数据与模型部署的工程演示。

### 技术栈

| 层 | 技术 | 开发环境 | 职责 |
|---|---|---|---|
| 数据预处理 | C++ (CLI) | macOS, Apple clang 17.0 | 数据适配、清洗、标准化、特征提取 |
| 模型训练 | Python | macOS（快速迭代）+ Google Colab（正式训练/GPU） | 训练、模型对比、适配层、Core ML 导出验证 |
| 模型格式 | Core ML (.mlpackage) | macOS | 训练产物与 macOS 之间的独立验证 seam |
| 应用界面 | SwiftUI + Swift Charts | macOS, Swift 6.1, Xcode 16.4 | macOS 本地推理与图表展示 |

### 本机工具链

以下均为 macOS arm64 本地环境（已确认可用）：

| 工具 | 版本 | 路径 | 状态 |
|---|---|---|---|
| **clang++** | Apple clang 17.0.0 | `/usr/bin/clang++` | ✅ 系统自带 |
| **make** | — | `/usr/bin/make` | ✅ 系统自带 |
| **CMake** | 未安装 | `brew install cmake` | ⬜ 需安装（可选，可用 Makefile 替代） |
| **Python 3** | 3.9.6 | `/usr/bin/python3` | ✅ 系统自带（版本较旧） |
| **uv** | 0.11+ | `brew install uv` | ⬜ Python 包/环境管理（替代 pip + venv），无需手动创建虚拟环境 |
| **Swift** | 6.1.2 | `/usr/bin/swift` | ✅ 系统自带 |
| **Xcode** | 16.4 | `/Applications/Xcode.app` | ✅ 含 macOS SDK 15.5 |

### 工具链决策

- **C++ 构建**：使用简单的 **Makefile** 而非 CMake。MVP 阶段的 C++ 代码规模小（几个源文件），Makefile 足够且零额外依赖。如果后期复杂度增加再引入 CMake。
- **Python 环境**：macOS 本地使用 **uv** 管理依赖和虚拟环境。uv 内置虚拟环境支持（`uv venv`），无需手动 `python3 -m venv`。Colab 端使用 `uv pip install` 或 `!pip install` 安装相同的 `requirements.txt`。Python 版本：本地使用系统 Python 3.9，Colab 使用其默认版本。MVP 只使用 numpy、pandas、scikit-learn、xgboost、lightgbm、coremltools，版本兼容性风险低。
- **Swift**：使用 Xcode 16.4 + Swift 6.1，最低部署目标 macOS 14.0（与 macOS SDK 15.5 兼容）。Swift Charts 是系统框架无需额外依赖。
- **Python 本地 vs Colab 的分工**：
  - 本地（macOS + uv）：快速迭代、数据探索、单元测试、Core ML 转换验证
  - Colab：正式训练（尤其需要 GPU 时）、多模型对比实验、评估报告生成
  - 关键约束：**同一份 `requirements.txt` 和训练脚本必须同时能在本地和 Colab 上运行**

### 当前状态

**MVP 阶段全部完成。** 四个 milestone 均已交付，全链路一致性验证通过。详见 [Section 8 文档治理](#8-文档治理)。

### 术语表

| 术语 | 定义 |
|---|---|
| **Night Session** | 单个睡眠事件（通常一晚），包含摘要行和可选的时序数据 |
| **Recovery Label** | MVP 主目标：三分类标签 — 0=差, 1=一般, 2=良好 |
| **Weak Label** | 基于规则推导的标签，非人工标注。当数据集缺少原生恢复标签时使用 |
| **Feature Package** | Swift 端推理输入：预计算的特征向量 + 展示用标准化时序 |
| **Golden Sample** | 固定的一份样本（标准化时序 + 特征包 + 期望预测），用于跨 C++/Python/Core ML/Swift 四层验证 |
| **Model Adapter** | 统一不同模型的特征顺序、标签映射、元数据和导出行为的适配层 |
| **Core ML Export** | 将训练产物转换为 macOS 可本地加载的 `.mlpackage` 的过程 |
| **Artifact Manifest** | 每个产物包携带的元数据：schema_version、列顺序、CSV 编码、标签枚举、特征顺序 |
| **Wearable Sleep CSV** | Kaggle 风格的可穿戴设备导出的睡眠数据，通常包含表格化的睡眠摘要和时序数据 |

---

## 2. Specification

### 2.1 Data Architecture

#### 已选数据集：Fitbit Fitness Tracker Data

数据已下载至仓库的 `Fitabase Data 3.12.16-4.11.16/` 目录。这是 Kaggle 上的 Bellabeat/Fitbit Fitness Tracker Data 的完整版本。

**实际数据概况（经检查确认）**：

| 指标 | 数值 |
|---|---|
| 总文件数 | 11 个 CSV |
| 时间范围 | 2016-03-11 ~ 2016-04-11（约 1 个月） |
| 有心率数据的用户 | 14 人 |
| 有睡眠数据的用户 | 23 人 |
| **同时有睡眠 + 心率的用户** | **仅 12 人** |
| 心率粒度 | 每秒（`heartrate_seconds_merged.csv`，1,154,681 行） |
| 睡眠粒度 | 每分钟（`minuteSleep_merged.csv`，198,560 行） |
| 活动粒度 | 每分钟（`minuteIntensitiesNarrow_merged.csv`，1,445,041 行） |
| 睡眠值编码 | 1=asleep, 2=restless, 3=awake |
| 活动强度编码 | 0/1/2/3 四档 |
| 心率范围 | 36 ~ 185 bpm，均值 79.8 bpm |

#### MVP 数据维度决策

基于上述数据实际情况，MVP 使用以下具体维度和文件：

**主链路（必须，有真实数据）**：

| 数据维度 | 来源文件 | 用途 |
|---|---|---|
| **睡眠状态** | `minuteSleep_merged.csv` | 计算入睡/起床时间、总时长、中断次数（awaken_count）、碎片化分数（sleep_fragmentation_score） |
| **活动强度** | `minuteIntensitiesNarrow_merged.csv` | 计算活动均值（activity_mean）、峰值次数（activity_spike_count） |
| **心率** | `heartrate_seconds_merged.csv` | 聚合到分钟级后提取所有心率特征（mean/min/max HR、hr_std、hr_drop_first_90m） |
| **每日摘要** | `dailyActivity_merged.csv` | 辅助验证和补充字段（步数、卡路里、活动分钟数等） |

**辅助链路（可选，用于丰富特征）**：

| 数据维度 | 来源文件 | 用途 |
|---|---|---|
| METs | `minuteMETsNarrow_merged.csv` | 可选的活动代谢当量，用于提升活动强度估计精度 |
| 步数 | `minuteStepsNarrow_merged.csv` | 可选的分钟级步数，替代/补充活动强度 |
| 卡路里 | `minuteCaloriesNarrow_merged.csv` | 可选的能量消耗估计 |

**核心约束**：
- **仅使用 12 个同时有心率和睡眠数据的用户**作为 MVP 主训练集
- 剩余 11 个仅有睡眠数据的用户可作为仅基于睡眠特征的辅助训练样本
- 心率数据从秒级聚合到分钟级（取每分钟均值），与睡眠数据时间对齐
- 时间戳格式：原始为 `M/D/YYYY H:MM:SS AM/PM`，需在 C++ 预处理中转换为 ISO 8601

**合成兜底数据**：

| 优先级 | 数据集 | 用途 |
|---|---|---|
| **兜底** | Sleep Health & Daily Performance Dataset (Kaggle) | 快速对照实验，弱标签逻辑预验证 |
| **中期** | Child Mind Institute - Detect Sleep States | 时序增强，暂不进入 MVP |

#### 四张核心表

**`night_sessions.csv`** — 每晚一行会话摘要

| 字段 | 类型 | 说明 |
|---|---|---|
| session_id | string | 唯一会话 ID，跨表关联主键 |
| user_id | string | 用户 ID |
| source_type | string | `synthetic` / `kaggle` / `manual` |
| sleep_start | datetime | 入睡时间 (ISO 8601) |
| sleep_end | datetime | 起床时间 (ISO 8601) |
| total_duration_min | float | 总睡眠时长（分钟） |
| awaken_count | int | 夜间中断次数 |
| recovery_label | int | 0=差, 1=一般, 2=良好 |

**`heart_rate_timeseries.csv`** — 每分钟心率观测

| 字段 | 类型 | 说明 |
|---|---|---|
| session_id | string | 关联会话 |
| timestamp | datetime | ISO 8601 |
| heart_rate | float | bpm |
| quality_flag | int | 0=正常, 1=插补, 2=异常修正 |

**`activity_timeseries.csv`** — 每分钟活动强度

| 字段 | 类型 | 说明 |
|---|---|---|
| session_id | string | 关联会话 |
| timestamp | datetime | ISO 8601 |
| activity_level | float | 归一化 0-1 |
| movement_event | int | 是否发生明显活动峰值，0/1 |

**`derived_features.csv`** — 每晚会话的模型输入特征（仅 MVP 所需字段）

| 字段 | 类型 | 说明 |
|---|---|---|
| session_id | string | 关联会话 |
| mean_hr | float | 夜间平均心率 |
| min_hr | float | 夜间最低心率 |
| max_hr | float | 夜间最高心率 |
| hr_std | float | 心率标准差 |
| hr_drop_first_90m | float | 前 90 分钟心率下降幅度 |
| activity_mean | float | 平均活动强度 |
| activity_spike_count | int | 活动峰值次数 |
| sleep_fragmentation_score | float | 睡眠碎片化分数 |
| total_duration_min | float | 睡眠总时长 |
| recovery_label | int | 分类标签 (0/1/2) |

#### 弱标签生成规则

当数据集没有原生恢复标签时，使用规则生成 `recovery_label`：

```text
recovery_score = 100
  - duration_penalty        (sleep < 360min 扣分)
  - mean_hr_penalty         (平均心率偏高 扣分)
  - hr_drop_penalty         (前90min心率下降不明显 扣分)
  - activity_penalty        (夜间活动峰值过多 扣分)
  - awaken_penalty          (中断次数过多 扣分)

recovery_score >= 75  →  recovery_label = 2 (良好)
50 <= score < 75     →  recovery_label = 1 (一般)
score < 50           →  recovery_label = 0 (差)
```

> 具体扣分权重需在实际数据上调试。如果 Kaggle 数据集包含可映射的睡眠质量字段，优先对比 score-mapping 与 weak-label 两种策略后锁定最终方案。

#### Artifact Manifest（跨平台契约）

每个产物包必须携带以下元数据：
- `schema_version`、列顺序、CSV 编码 (UTF-8)
- 时间戳格式 (ISO 8601)、标签枚举 (0/1/2)
- 模型输入特征顺序、数据来源标记 (`source_type`)
- 模型版本标识

### 2.2 Processing Pipeline

四层架构，每层有明确的职责边界：

```
┌─────────────────────────────────────────────────────┐
│ C++ CLI                                              │
│ 原始 Kaggle CSV → 字段映射 → 标准化 → 特征提取       │
│ 输出: night_sessions.csv, *_timeseries.csv,          │
│       derived_features.csv                           │
├─────────────────────────────────────────────────────┤
│ Python / Google Colab                                 │
│ 读取 feature package → 训练 3 类模型 → 模型适配层    │
│ → Core ML 导出验证                                    │
│ 输出: 模型产物, 评估报告, .mlpackage                  │
├─────────────────────────────────────────────────────┤
│ Core ML Export (独立验证 seam)                        │
│ coremltools 转换 → 输入输出对齐 → 一致性验证          │
│ 输出: 可部署 .mlpackage, 转换问题记录                  │
├─────────────────────────────────────────────────────┤
│ SwiftUI macOS App                                     │
│ 加载 feature package → 本地 Core ML 推理 → 图表展示  │
│ 输出: 恢复等级, 概率, 影响因素, 趋势图                 │
└─────────────────────────────────────────────────────┘
```

**关键约束**：Swift 端不做原始时序特征提取，只消费预计算的 feature package 和展示用标准化时序。

### 2.3 Feature Engineering (MVP)

**9 个 MVP 特征**（Feature Package 中的必需字段，按模型输入顺序排列）：

| # | 特征名 | 说明 |
|---|---|---|
| 1 | mean_hr | 夜间平均心率 |
| 2 | min_hr | 夜间最低心率 |
| 3 | max_hr | 夜间最高心率 |
| 4 | hr_std | 心率标准差 |
| 5 | hr_drop_first_90m | 前 90 分钟心率下降幅度 |
| 6 | activity_mean | 平均活动强度 |
| 7 | activity_spike_count | 活动峰值次数 |
| 8 | sleep_fragmentation_score | 睡眠碎片化分数 |
| 9 | total_duration_min | 睡眠总时长 |

> **范围澄清**：`project-plan.md` 额外列出了 4 个分段特征（前/后半夜平均心率、高活动片段数量、夜间中断次数作为独立特征），这些属于中期增强，不进入 MVP。夜间中断次数 `awaken_count` 已经在 `night_sessions.csv` 中保留，弱标签生成时会用到，但它不作为独立的模型输入特征。`fatigue_score`（回归标签）同样不进入 MVP 验收范围。

### 2.4 Model Requirements

**三类候选分类器**（`recovery_label` 三分类）：

| 模型 | 角色 |
|---|---|
| XGBoost / LightGBM | **主部署候选** — 展示真实模型部署权衡 |
| Logistic Regression | 对比基线 + Core ML 回退路径 |
| Random Forest | 对比基线 + Core ML 回退路径 |

**模型适配层职责**：统一特征顺序、标签映射、模型元数据、导出前中间表示。不同模型通过同一适配层训练和导出。

**Core ML 导出路线**：
1. 主线：XGBoost/LightGBM → coremltools → .mlpackage
2. 回退：Logistic Regression 或 Random Forest → coremltools → .mlpackage
3. 如果主线回退，回退决策必须文档化为工程权衡，不能隐式简化

### 2.5 macOS App Requirements

**单夜分析页**（Ticket 05）：
- 心率趋势图（整夜时间序列）
- 活动趋势图（整夜时间序列）
- 恢复等级卡片（0/1/2 + 语义标签）
- 概率/置信度展示
- 关键影响因素列表（来源：模型特征贡献 或 离线预计算解释）

**多夜趋势视图**（Ticket 06）：
- 多夜恢复输出变化趋势
- 与单夜视图使用一致的术语和标签映射
- 不破坏单夜主流程

**输入约束**：
- 只加载预计算 feature package（不做端侧特征提取）
- 加载展示用标准化时序（用于趋势图渲染）

### 2.6 Testing Strategy

**核心原则**：测外部可观测行为和高层 seam，不测内部实现细节。

| Seam | 测试重点 |
|---|---|
| C++ 预处理 | 输出 CSV 的 schema 一致性、特征数值可复现 |
| Python 训练 | 特征加载可复现、标签映射稳定、指标可重复输出、模型元数据产出 |
| Core ML | 导出成功/失败特征化、输出 shape 验证、与 Python 数值一致性（容差内） |
| SwiftUI | 加载 feature package、产生推理结果、渲染预期图表和结果区域 |

**黄金样本（Golden Sample）**：一份固定的单夜标准化时序 + 特征包 + Python 期望预测结果，作为 C++、Python、Core ML、Swift 四层的公共回归参考。

### 2.7 Out of Scope

- 医学诊断或临床级健康建议
- 完整睡眠分期建模
- 实时端侧原始流特征提取
- iOS 部署
- Apple Watch 等硬件接入
- 个性化基线建模（跨长周期用户历史）
- GitHub Issues 同步（当前本地 markdown 阶段）
- 回归任务（`fatigue_score`）不进入 MVP 主验收

---

## 3. Milestones

### 概览

```
M1: Data Foundation      →  Ticket 01
M2: Preprocessing         →  Ticket 02
M3: Training & Adapter    →  Tickets 03 + 04
M4: macOS & Delivery      →  Tickets 05 + 06 + 07
```

各 milestone 之间为严格前置依赖关系。

---

### M1: Data Foundation（数据打底）

**对应 Ticket**: [01-lock-data-contract-and-golden-sample](.scratch/sleep-recovery-mvp/issues/01-lock-data-contract-and-golden-sample.md)

**目标**：锁定数据契约、Kaggle 数据集选择、字段映射、artifact manifest 和黄金样本，为后续模块提供稳定输入。

**具体工作**：
1. 固定 4 张核心表的字段名、类型、单位和主键关系
2. ~~下载并检查 Bellabeat/Fitbit Kaggle 数据集的实际文件结构~~ ✅ 已完成：数据在 `Fitabase Data 3.12.16-4.11.16/`，11 个 CSV，已分析结构和覆盖情况
3. 设计 Fitbit 原始字段到统一 schema 的映射方案（睡眠值 1/2/3 → asleep/restless/awake；活动强度 0/1/2/3 → 归一化 0-1；心率秒级 → 分钟级聚合）
4. 锁定弱标签 scoring formula 的具体权重参数
5. 产出 artifact manifest（schema_version、列顺序、CSV 编码、标签映射）
6. 定义黄金样本包：从 12 个有心率+睡眠的用户中选取 1 晚 → 标准化时序 + 特征包 + 期望预测结果（人工标注）
7. 为每张表准备少量人工可读的样例 CSV（从实际 Fitbit 数据中抽取）

**验收标准**：
- 后续模块不需要再修改字段命名和表结构
- 标签定义在训练和展示阶段不会产生歧义
- Fitbit 数据字段映射方案已完成，明确了原始编码到统一 schema 的转换规则
- 黄金样本包足以支撑后续 C++、Python、Core ML、Swift 四层一致性验证

**风险**：12 个用户样本量较小，可能限制模型泛化能力 → 明确这不是医疗应用；M3 训练阶段通过交叉验证和模型选择评估此风险

---

### M2: Preprocessing Pipeline（预处理跑通）

**对应 Ticket**: [02-build-single-night-preprocessing-to-feature-package](.scratch/sleep-recovery-mvp/issues/02-build-single-night-preprocessing-to-feature-package.md)

**依赖**: M1

**目标**：完成 C++ 数据适配、标准化、合成补齐和特征提取 CLI，产出统一 feature package。

**具体工作**：
1. 搭建 `cpp/` 目录和构建配置（CMake 或 Makefile）
2. 实现 Kaggle 数据适配模块：原始字段读取 → 时间戳归一化 → 单位归一化 → 缺失值/异常值处理
3. 实现合成数据生成器：心率时序（入睡下降 + REM 波动 + 觉醒扰动 + 噪声）、活动时序（低基线 + 峰值）
4. 实现 9 个 MVP 特征的提取逻辑
5. 实现标准化输出：`derived_features.csv` + 展示用标准化时序
6. 让黄金样本能从原始输入稳定生成同结构输出
7. 确保 Kaggle 数据和合成数据都能落到同一 feature package 结构

**验收标准**：
- 同一输入配置稳定生成同结构输出
- 输出 CSV 可被 Python `pandas` 直接读取
- 9 个 MVP 特征全部正确写出，无字段缺失或跨表关联断裂
- Kaggle 和合成数据收敛到同一 feature package 结构

**风险**：Kaggle 原始数据与目标 schema 差异大 → 适配逻辑与合成补齐逻辑分层，结束时形成可审阅的问题清单

---

### M3: Training & Model Adapter（训练、适配与评估）

**对应 Tickets**: [03-build-baseline-training-and-model-adapter](.scratch/sleep-recovery-mvp/issues/03-build-baseline-training-and-model-adapter.md) + [04-validate-coreml-primary-route-and-fallback](.scratch/sleep-recovery-mvp/issues/04-validate-coreml-primary-route-and-fallback.md)

**依赖**: M2

**目标**：在 Colab 上完成多模型训练、对比、适配层设计，并验证 Core ML 主线导出可行性。

**具体工作**：
1. 搭建 `python/` 目录，创建 Colab notebook
2. 读取 `derived_features.csv`，拆分 train/val/test
3. 训练 3 类模型（Logistic Regression、Random Forest、XGBoost/LightGBM）
4. 比较多维度：分类效果、特征可解释性、Core ML 转换复杂度、部署风险
5. 设计并固定模型适配层：统一特征顺序、标签映射、模型元数据、导出配置
6. 以 XGBoost/LightGBM 为主部署候选，完成至少一次 Core ML 转换实验
7. 输出评估结果（准确率、混淆矩阵、特征重要性）和模型取舍分析

**验收标准**：
- 训练脚本能从 feature table 完整跑通，评估指标可重复输出
- 模型输入特征顺序已锁定
- 至少完成一次 XGBoost/LightGBM → Core ML 的可行性验证
- 明确选出一版主部署模型，对比模型与主模型的取舍理由已成文

**风险**：XGBoost/LightGBM Core ML 转换存在结构或兼容性阻碍 → 在本 milestone 内完成归因，提前暴露问题而非拖到 Swift 集成阶段

---

### M4: macOS Deployment & Delivery（端侧部署与交付）

**对应 Tickets**: [05-deliver-single-night-macos-analysis-screen](.scratch/sleep-recovery-mvp/issues/05-deliver-single-night-macos-analysis-screen.md) + [06-deliver-multi-night-trend-view](.scratch/sleep-recovery-mvp/issues/06-deliver-multi-night-trend-view.md) + [07-close-mvp-validation-and-delivery-pack](.scratch/sleep-recovery-mvp/issues/07-close-mvp-validation-and-delivery-pack.md)

**依赖**: M2, M3

**目标**：完成 Core ML 接入和 macOS 端分析展示，形成最终 MVP 闭环并补齐交付文档。

**具体工作**：
1. 搭建 `swift-app/` macOS 应用骨架（SwiftUI + Swift Charts）
2. 加载 M3 产出的 Core ML 模型，实现本地推理调用
3. 实现单夜分析页：心率趋势图、活动趋势图、恢复等级卡片、概率展示、关键影响因素
4. 实现多夜趋势视图：多夜恢复输出变化、与单夜术语一致
5. 使用黄金样本完成 Python vs Core ML vs Swift 三层推理一致性验证
6. 输出 Core ML 优化分析（输入类型、输出结构、量化/精度取舍、与对比模型的部署复杂度差异）
7. 收口 MVP 交付包：一致性验证结论、模型主线/回退权衡说明、演示说明、spec 与 tickets 一致性检查

**验收标准**：
- Swift 端可成功完成单夜数据推理
- Swift 推理结果与 Python 侧预测一致（黄金样本验证）
- 界面至少包含 2 条趋势图和 2 个结果区域
- 从输入到输出形成完整离线闭环
- 关键影响因素有明确的数据来源和解释方式

**风险**：Core ML 转换后输入输出与训练侧不一致 → 以黄金样本做基线校验，加入 Python/Swift 一致性比对

---

## 4. Dependency Graph

```
                    M1: Data Foundation
                    │     Ticket 01
                    ▼
                    M2: Preprocessing
                    │     Ticket 02
                    ├──────────────┐
                    ▼              │
         M3: Training & Adapter   │
         Tickets 03 + 04          │
                    │              │
                    ├──────────────┤
                    ▼              ▼
         M4: macOS & Delivery
         Tickets 05 + 06 + 07
```

**关键依赖说明**：

- **M2 依赖 M1**：schema 不确定则预处理无法开始
- **M3 依赖 M2**：无 feature package 则无训练输入
- **M4 依赖 M2 + M3**：需要展示用标准化时序（来自 M2）+ Core ML 模型（来自 M3）
- **Ticket 05 有双重依赖**：需要 Ticket 02（feature package 格式）和 Ticket 04（Core ML 模型）两者都可用后才能完整实现单夜推理和展示。实际执行时可先基于 M2 构建 UI 骨架和图表渲染，等 M3 产出模型后接入推理
- **Ticket 06 依赖 05**：多夜视图在单夜视图基础上扩展
- **Ticket 07 依赖 04 + 05 + 06**：最终交付需要所有前置工作完成
- **Ticket 04 可在 Ticket 03 部分完成时启动**：模型适配层确定、主候选模型选定后即可开始 Core ML 导出实验
- **Golden Sample** 在 M1 定义，在 M2/M3/M4 中作为跨层验证的公共回归参考

---

## 5. Gap Analysis

以下是开始实现前需要解决的关键待定事项：

| # | Gap | 影响范围 | 建议 |
|---|---|---|---|
| 1 | ~~Bellabeat/Fitbit Kaggle 数据集未实际下载检查~~ ✅ | M1 | 数据已下载，11 个 CSV 文件结构已分析，12 个同时有心率+睡眠的用户确认为主训练集 |
| 2 | XGBoost/LightGBM Core ML 转换兼容性未知 | M3 | M3 必须完成至少一次转换实验并记录结果，这是 MVP 最大的技术风险 |
| 3 | 弱标签 scoring formula 的扣分权重未定 | M1 | 需在实际数据分布上调试，可在 M1 先设定初值、M3 训练阶段调整 |
| 4 | 合成数据生成的具体参数未锁定 | M2 | M1 结束时给出 200+ 晚合成会话的目标规范；12 个用户的样本量较小，合成数据增强尤为重要 |
| 5 | macOS app 实现方式未锁定 | M4 | 建议纯 SwiftUI + Swift Charts，不引入 AppKit 复杂度 |
| 6 | 关键影响因素的实现方式未定（模型贡献 vs 离线预计算）| M4 | 取决于 M3 选定的主模型是否支持特征贡献提取 |
| 7 | GitHub 远程仓库尚未连接 | 交付 | M4 收口阶段可以考虑推送到 `git@github.com:tangzzz-fan/sleep-recovery.git` |
| 8 | 睡眠值编码 2（restless）的含义和处理方式待确认 | M1 | 在弱标签规则中需明确 restless 算作 asleep 还是 awake |
| 9 | 12 个用户中仅有 1 个用户（2026352035）仅有 1 天心率数据 | M1 | 该用户需从主训练集中排除或标记为辅助样本 |

---

## 6. 关键文档索引

| 文档 | 用途 |
|---|---|
| [spec.md](.scratch/sleep-recovery-mvp/spec.md) | 完整 user stories 和实现决策（英文） |
| [project-plan.md](docs/sleep-recovery-project-plan.md) | 完整项目方案含中期和远期规划 |
| [milestone-plan.md](.trae/documents/mvp-milestone-plan.md) | 详细的里程碑分计划含风险控制 |
| [ADR-0001](docs/adr/0001-mvp-data-and-deployment-shape.md) | 架构决策记录 |
| [data_readme.md](data_readme.md) | 数据集选择详细分析 |
| [CONTEXT.md](CONTEXT.md) | 术语表和架构形状 |
| [Tickets](.scratch/sleep-recovery-mvp/issues/) | 7 个可执行工单 |

---

## 7. 下一步

从 **Ticket 01 — 锁定数据契约与黄金样本** 开始执行。这是整个 MVP 的基础，完成后后续 C++、Python、Core ML 和 Swift 工作都会有稳定的输入契约可依赖。

---

## 8. 文档治理

### 与现有文档的关系

本文档是 MVP 阶段的**权威参考**。与旧文档冲突时，以本文档为准。现有文件保留用途：

| 文档 | 保留用途 |
|---|---|
| `.scratch/.../spec.md` | 完整 20 条 user stories 和实现决策细节 |
| `docs/.../project-plan.md` | 中期和远期阶段规划（MVP 后） |
| `.trae/.../mvp-milestone-plan.md` | 每个 milestone 的细化工作分解 |
| `docs/adr/0001-...` | 架构决策的正式记录 |
| `.scratch/.../issues/*.md` | 每个工作单元的可操作检查清单 |
| `data_readme.md` | 完整的数据集候选分析（3 个方案及利弊） |

### 更新策略

- MVP 期间如有重大范围变更，更新本文档
- 各 milestone 完成时更新状态
- 出现新的架构决策时，通过新 ADR 或更新 Section 2 捕获
- MVP 收口时，确认本文档与 7 个 tickets 的一致性（Ticket 07 验收标准之一）
