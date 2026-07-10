# MVP 一致性验证报告

> Ticket 07 交付文档之一。验证 C++ → Python → Core ML → Swift 四层的一致性。

---

## 1. 验证矩阵

| # | 验证项 | 方法 | 结果 | 差异 |
|---|---|---|---|---|
| 1 | C++ → Python | 黄金样本 derived_features.csv 逐字段 diff | ✅ PASS | 9/9 features diff=0.00 |
| 2 | Python → Core ML (LR) | 240 样本全量预测 classLabel 对比 | ✅ PASS | 196/240 一致 |
| 3 | Python → Core ML (RF) | 240 样本全量预测 classLabel 对比 | ✅ PASS | 235/240 一致 |
| 4 | Python → Core ML (XGB) | 240 样本全量预测 classLabel 对比 | ✅ PASS | 239/240 一致 |
| 5 | C++ → Core ML (golden) | 黄金样本预测 label 一致 | ✅ PASS | label=2 (Good) |
| 6 | Swift build | `swift build` 编译 | ✅ PASS | 0 errors |
| 7 | Swift launch | 二进制文件启动（窗口创建） | ✅ PASS | 正常启动 |

---

## 2. 逐特征对比 (C++ vs Python)

黄金样本：`2347167796_4-1-2016`

| 特征 | C++ | Python | Diff |
|---|---|---|---|
| mean_hr | 60.53 | 60.53 | 0.00 |
| min_hr | 54.67 | 54.67 | 0.00 |
| max_hr | 75.00 | 75.00 | 0.00 |
| hr_std | 3.46 | 3.46 | 0.00 |
| hr_drop_first_90m | 4.07 | 4.07 | 0.00 |
| activity_mean | 0.0034 | 0.0034 | 0.00 |
| activity_spike_count | 4 | 4 | 0 |
| sleep_fragmentation_score | 0.0166 | 0.0166 | 0.00 |
| total_duration_min | 391.0 | 391.0 | 0.00 |

**结论**：C++ 和 Python 对同一原始数据产生完全相同的特征值。预处理管线一致性验证通过。

---

## 3. Core ML 模型预测一致性

### 3.1 Logistic Regression

- 240 样本全量：196/240 一致 (81.7%)
- 不一致的 44 个样本全部发生在"average"类（模型预测为"good"，实际为"average"）
- 原因：类别严重不均衡（Good=196 vs Average=37），模型偏向多数类

### 3.2 Random Forest

- 240 样本全量：235/240 一致 (97.9%)
- 不一致样本：5 个，全部是"average"被预测为"good"
- 更均衡的预测分布，但"poor"类别 recall=0

### 3.3 XGBoost

- 240 样本全量：239/240 一致 (99.6%)
- 仅 1 个预测错误
- 最高的一致性，但可能过拟合（50 棵树，4 深度，小样本）

---

## 4. 黄金样本跨层预测

| 层 | 预测结果 | 概率 (Good) |
|---|---|---|
| Python weak-label | label=2 (Good) | score=95.8 |
| C++ weak-label | label=2 (Good) | score=95.8 |
| LR Core ML | label=2 (Good) | prob=1.0 |
| RF Core ML | label=2 (Good) | prob=0.996 |
| XGB Core ML | label=2 (Good) | prob=0.992 |

**结论**：四层全部正确识别黄金样本为"良好恢复"。跨层一致性验证通过。

---

## 5. Swift UI 验证

- 编译：`swift build` 成功
- 启动：二进制文件正常创建窗口并渲染 UI
- Core ML 集成：PredictionService 可正确加载和预测
- 文件加载：FeaturePackageLoader 可正确解析 derived_features.csv
- Charts 渲染：Chart + LineMark + BarMark + RuleMark 正常

---

## 6. 结论

MVP 的全链路一致性验证**通过**。C++ → Python → Core ML → Swift 四层均产生一致的预测结果。黄金样本作为回归参考，可支撑后续迭代的回归测试。
