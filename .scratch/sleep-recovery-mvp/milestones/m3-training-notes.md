# M3: Training & Model Adapter — 知识难点与解决方案

> 对应 Tickets 03 + 04。训练 3 类模型、设计模型适配层、验证 Core ML 导出。
> **状态：已实施完成**（3 模型训练 + Core ML 导出全部验证通过）。

## 实际踩坑记录

### 坑 1: coremltools 9.0 不兼容 scikit-learn 1.6

**现象**：安装 coremltools 9.0 + scikit-learn 1.6.1 后，sklearn converter 被禁用。
**修复**：降级到 scikit-learn 1.5.2 + coremltools 8.3。

### 坑 2: coremltools 仅支持 OvR 多分类

**现象**：使用 `multi_class='multinomial'` 时报错 "Only One Vs Rest is supported"。
**修复**：改用 `multi_class='ovr'`，对 3 分类任务性能无影响。

### 坑 3: coremltools 预测输入格式

**现象**：传入 `{"input": array}` 报错 KeyError "input 不是模型的输入名"。
**修复**：需要按特征名逐个传参：`{"mean_hr": 60.5, "min_hr": 54.7, ...}`。

### 坑 4: XGBoost 的 OpenMP 依赖

**现象**：`import xgboost` 时报错 libomp.dylib 未加载。
**修复**：`brew install libomp`。

### 坑 5: XGBoost Core ML 转换需要 DMatrix feature_names

**现象**：`coremltools.converters.xgboost.convert()` 报错 "training data did not have the following fields"。
**修复**：`dtrain = xgb.DMatrix(X, label=y, feature_names=FEATURE_ORDER)`。

### 坑 6: RandomForest Core ML 输出概率异常

**现象**：RF 的 Core ML 输出概率是 Python 的 100 倍（如 95.9% 而非 0.959）。
**原因**：coremltools 的 RandomForest 转换器输出的是百分比概率（0-100）而非小数（0-1）。
**处理**：不影响 classLabel 预测，知道即可。

## 预判验证结果

| 预判 | 是否命中 | 实际情况 |
|---|---|---|
| 1. 12 用户样本量限制 | ✅ 命中 | Poor 类仅 7 样本，RF 在该类 recall=0 |
| 2. XGBoost Core ML 兼容性 | ✅ 命中 | 需要 libomp + DMatrix feature_names，但最终成功导出 |
| 3. 模型适配层设计 | ✅ 命中 | ModelAdapter 统一了 LR/RF 的训练和预测接口 |
| 4. Python 版本兼容性 | ✅ 命中 | scikit-learn 1.6→1.5 兼容 coremltools 8.3 |
| 5. 评估指标陷阱 | ✅ 命中 | Accuracy=0.946 但 BalancedAcc=0.69，类别不均衡导致假象 |
| 6. Colab 连接稳定性 | ❌ 未测试 | 全部训练在本地完成（240 样本太小，无需 Colab GPU） |
| 7. coremltools 版本记录 | ✅ 实施 | 训练报告 JSON 包含所有元数据 |
| 8. 特征重要性一致性 | ⚠️ 部分命中 | 3 模型特征重要性排序不同（预期行为），已记录在 model-selection-analysis.md |
| 9. 输入输出一致性 | ✅ 命中 | Python/CoreML/Swift 三端命名一致，黄金样本验证通过 |
| 10. 训练-测试泄露 | ✅ 命中 | 弱标签和特征强相关，但这是预期行为（非医学诊断） |

---

## 1. 12 个用户的样本量限制

**核心问题**：12 个用户 × 14-31 天 ≈ 约 200-350 晚的可用数据。
这在机器学习中是非常小的样本量。

**影响**：
- 三分类问题，每类 60-100 个样本 → 容易过拟合
- 随机森林可能比 XGBoost 更适合（决策树在小样本上表现更好）
- 深度学习（CNN/LSTM）完全不具备条件

**缓解措施**：
- 使用 **Leave-One-User-Out (LOUO)** 交叉验证，而不是随机拆分
- 优先选择特征数量不超过 1/10 样本数的模型（即 ≤ 20 个特征）
- 强正则化：Logistic Regression 用 L2，Random Forest 限制树深度
- 合成数据增强：从 12 个用户的分布中采样生成 200+ 晚合成数据

**关键决策**：样本量太小是事实，不是 bug。MVP 的目标是建立 pipeline，
不是达成高准确率。文档中必须诚实说明。

---

## 2. XGBoost/LightGBM Core ML 转换兼容性

**预判问题**：这是整个 MVP **最大的技术风险**。

**已知问题**：
- `coremltools` 对 XGBoost 的支持从 5.0 版本开始改善，但只支持 TreeEnsemble 模型
  （不支持 multiclass logistic objective 的直接转换）
- LightGBM 的 `.txt` 模型导出到 Core ML 需要中间转换步骤
- 三分类的 `objective="multi:softprob"` 会产生 3 个概率输出，
  Core ML 需要显式声明 MultiArray 输出

**推荐验证流程**（M3 必须做）：
1. 在本地训练一个最小 XGBoost/LightGBM 模型（5 棵树，3 个特征，仅用于验证转换）
2. 用 `coremltools.converters.xgboost.convert()` 测试导出
3. 检查输出格式：输入名称、参数类型、概率输出维度
4. 如果 XGBoost 转换失败，尝试 LightGBM → ONNX → Core ML 路径
5. 如果两者都失败，直接启用 Logistic Regression 作为主模型

**回退路径**（已锁定）：
- Logistic Regression → scikit-learn → coremltools → .mlpackage
- Random Forest → scikit-learn → coremltools → .mlpackage
- 两者都经过充分测试，稳定可靠

**关键**：无论主线是否成功，都要写一份问题分析记录，解释为什么选择 XGBoost/LightGBM
或为什么回退。

---

## 3. 模型适配层的接口设计

**目的**：让 Logistic Regression、Random Forest、XGBoost/LightGBM 三个模型
共享相同的训练/导出接口。

**推荐设计**（Python）：
```python
class ModelAdapter:
    def __init__(self, feature_order: list[str], label_map: dict):
        self.feature_order = feature_order
        self.label_map = label_map  # {0: "poor", 1: "average", 2: "good"}

    def prepare_features(self, df: pd.DataFrame) -> np.ndarray:
        """确保特征按固定顺序输出"""
        return df[self.feature_order].values

    def train(self, X, y, model_type: str) -> Any:
        """返回训练好的模型"""

    def export_metadata(self, model, path: str) -> dict:
        """导出模型元数据 JSON"""
```

**关键**：特征顺序必须和 `data-contract.md` 的 artifact manifest 一致，
否则 Swift 端加载特征包时顺序错位 → silent failure。

---

## 4. Colab vs 本地 macOS 的 Python 版本兼容性

**问题**：本地 macOS Python 3.9.6，Colab 默认用 Python 3.10+。
`requirements.txt` 必须同时兼容两个版本。

**缓解措施**：
- `numpy` 用 `>=1.24,<2.0`（兼容 Python 3.9-3.12）
- `scikit-learn` 用 `>=1.3`（保持稳定 API）
- `xgboost` 用 `>=2.0`（支持 coremltools 5.0 转换）
- `lightgbm` 用 `>=4.0`
- `coremltools` 用 `>=7.0`（macOS 本地）

**注意事项**：Colab 上不要用 `pip install coremltools`（macOS only），
本地 macOS 上运行 Core ML 验证。

---

## 5. 三分类评估指标的陷阱

**问题**：样本量小 + 类别可能不均衡 → 准确率（accuracy）会误导。

**推荐指标**：

| 指标 | 用途 | 注意 |
|---|---|---|
| **Balanced accuracy** | 考虑类别不均衡 | 每类的 recall 取平均 |
| **Macro F1** | 同等重视三个类别 | 三个类别都重要，没有"不重要"的类别 |
| **Confusion matrix** | 可视化具体错误 | 差→良好 vs 良好→差的误判，前者更严重 |
| **Per-class precision/recall** | 各分类的详细性能 | 优先保证极端预测准确（label=0 和 2） |

**不推荐**：单纯用 accuracy（在类别不均衡时可能达到 80% 的假象）。

---

## 6. Google Colab 的连接稳定性

**预判问题**：Colab 的免费 GPU 会因空闲断连，训练脚本可能被中断。

**推荐方案**：
- 训练脚本支持 checkpoint 保存（每 10 个 epoch 或每折交叉验证完成时）
- 使用 Colab 的 `files.download()` 导出模型到本地
- 或者在 Colab 上先跑一个轻量版（20% 数据），在本地跑完整版

---

## 7. coremltools 版本与模型参数的记录

**要求**：M3 产出的每个模型必须包含以下信息在元数据中：

```json
{
  "model_name": "xgboost_v1",
  "training_date": "2026-07-15",
  "framework": "xgboost",
  "framework_version": "2.0.3",
  "coremltools_version": "7.2",
  "feature_order": [...],
  "training_samples": 280,
  "validation_split": "LOUO",
  "accuracy": 0.72,
  "macro_f1": 0.68
}
```

这些信息会写进 `models/` 目录的元数据文件，以及 artifact manifest。

---

## 8. 模型特征重要性的一致性检查

**问题**：三个模型的特征重要性排序可能不一致（LR 系数 vs RF 特征重要性 vs XGB gain）。

**预判影响**：如果三个模型对"最关键特征"的判断不同，
这本身就是一个有价值的发现（而非 bug）。
在 MVP 文档中记录这种不一致，作为模型选择分析的一部分。

**推荐**：用 SHAP 值（`shap` 库）做统一的特征重要性计算，跨模型可比。
但 SHAP 计算在小样本上不稳定 → 如果结果不一致，只比较 XGBoost 和 Random Forest 的
built-in importance，不同时用 SHAP。

---

## 9. Core ML 输入输出的一致性问题

**预判问题**：Core ML 模型的输入名称必须和 Swift 端加载特征包时的键名完全一致。

**推荐命名规范**（在 M3 输出中固定）：
```
输入: mean_hr, min_hr, max_hr, hr_std, hr_drop_first_90m,
      activity_mean, activity_spike_count, sleep_fragmentation_score,
      total_duration_min
输出: recovery_probability (3-element MultiArray), recovery_label (int)
```

**Swift 端验证代码**：
```swift
let input = try MLMultiArray(shape: [1, 9], dataType: .float32)
input[0] = NSNumber(value: features.mean_hr)
// ... 按模型适配层的特征顺序填充
let output = try model.prediction(input: input)
```

**坑**：如果 Python 训练时的特征顺序是 [a,b,c] 而 Swift 端加载的是 [b,a,c]，
模型会正常运行但产生错误输出——没有报错，silent failure。
黄金样本必须用于验证。

---

## 10. 训练-测试泄露的风险

**预判问题**：如果弱标签基于 sleep_duration、mean_hr 等特征生成，
而模型又用这些特征进行训练，会产生目标泄漏（target leakage）。

**影响程度**：
- 弱标签本身就是"人为创造"的 label → 模型学到的是评分规则，而非真实恢复结论
- 这实际上是**预期行为**：MVP 不要求医学精度，只需要可复现的 pipeline

**文档化要求**：
- 在 M3 的模型选择分析中明确说明此问题
- 不过度解读模型的"高准确率"
- 在中期阶段可以引入真实的用户反馈标签来减少泄漏
