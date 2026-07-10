# 模型选择与对比分析

> Ticket 07 交付文档之二。记录 3 个候选模型的取舍决策和工程权衡。

---

## 1. 候选模型总览

| 模型 | 类型 | Balanced Acc | Macro F1 | Core ML 导出 | 训练速度 | 推理速度 |
|---|---|---|---|---|---|---|
| Logistic Regression | 线性 | 0.69 | 0.736 | ✅ 原生支持 | <1s | <0.1ms |
| Random Forest | 树集成 | 0.602 | 0.596 | ✅ pipeline convert | ~1s | <0.5ms |
| XGBoost | 梯度提升 | — | — | ✅ 已验证 | ~1s | <0.3ms |

---

## 2. 分类性能分析

### 混淆矩阵

**Logistic Regression**：
```
        Predicted
         Poor  Avg  Good
Actual Poor   2    4     1
       Avg    1   29     7
       Good   0    0   196
```

**Random Forest**：
```
        Predicted
         Poor  Avg  Good
Actual Poor   0    6     1
       Avg    0   30     7
       Good   0    1   195
```

### 关键发现

1. **两个模型都无法准确识别 "Poor" (差) 类别**。
   原因是训练数据中 Poor 样本太少（7/240 = 2.9%）。
   这不是模型的问题，是数据分布的问题。

2. **Random Forest 在 Poor 类上完全失效**（recall=0）。
   LR 至少能识别 2/7 的 Poor 样本（recall=0.286）。

3. **两个模型的宏平均 F1 都不高**（LR=0.736, RF=0.596），
   说明类别不均衡严重影响了性能。但在 12 用户/240 样本的小数据集上，
   0.7+ 的 F1 已经远超随机基线（0.333）。

4. **高 Accuracy 是假象**（LR=0.946, RF=0.938），
   因为 81.7% 的样本都是 Good 类别——任何模型只要预测"Good"就能达到 80%+ 的 accuracy。

---

## 3. Core ML 导出可行性

| 模型 | Core ML 兼容性 | 导出方式 | 注意事项 |
|---|---|---|---|
| Logistic Regression | ✅ 原生 | `ct.converters.sklearn.convert(model, ...)` | 必须使用 OvR 多分类；scikit-learn 1.5.2 + coremltools 8.3 |
| Random Forest | ✅ 通过 pipeline | `ct.converters.sklearn.convert(model, ...)` | 需要 sklearn Pipeline 包装 Scaler |
| XGBoost | ✅ 已验证 | `ct.converters.xgboost.convert(model, feature_names=..., ...)` | 必须安装 libomp；DMatrix 必须设置 feature_names；推荐 v2.1.4 |

**XGBoost Core ML 导出关键坑**：
- macOS 需 `brew install libomp`，否则 xgboost 无法加载
- DMatrix 必须传 `feature_names` 参数，否则 coremltools 报错 "training data did not have the following fields"
- coremltools 警告 "XGBoost 2.1.4 has not been tested" 实际不影响导出

---

## 4. 最终决策

### 主部署模型：Logistic Regression

**选择理由**：
1. **最高 Balanced Accuracy (0.69)** 和 **最高 Macro F1 (0.736)**，
   在三类别中表现最均衡
2. **Core ML 导出最简单**（sklearn 直接支持，无需 libomp）
3. **模型大小最优**（~10KB vs XGBoost ~50KB vs RF ~500KB）
4. **可解释性最好**（线性系数可直接解读每个特征的影响方向）
5. **在小样本上更稳定**（L2 正则化 + multinomial 比树模型更不易过拟合）

### 回退模型：Random Forest

**保留原因**：
- Core ML 集成经过充分测试和验证（97.9% 预测一致性）
- 供中期样本量扩充后参考

### XGBoost 状态

虽然 Core ML 导出已成功（7/10 夜，libomp 安装后），
但并未进行完整的 LOUO CV 评估（仅用于验证 Core ML 转换可行性）。
**保留为中期模型增强的参考路径**。

---

## 5. 工程权衡总结

| 维度 | LR | RF | XGB |
|---|---|---|---|
| 小样本适应性 | ✅ 最好 | 一般 | ⚠️ 易过拟合 |
| 类别不均衡处理 | ✅ 相对最好 | ❌ Poor 类失踪 | — |
| Core ML 集成 | ✅ 原生 | ✅ 需 Pipeline | ⚠️ 需 libomp |
| 可解释性 | ✅ 线性系数 | 特征重要性 | SHAP 值 |
| 跨平台一致性 | ✅ 100% | 97.9% | — |
| 模型尺寸 | 10KB | 500KB | 50KB |

**MVP 结论**：Logistic Regression 是所有维度中最适合当前 MVP 的模型。
Random Forest 和 XGBoost 为中期升级提供了已验证的技术路径。
