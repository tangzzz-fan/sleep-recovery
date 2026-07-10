# Swift App 模型与数据使用说明

## 1. 当前 Swift App 实际会加载哪些模型

`swift-app` 当前会按以下顺序在包资源中查找模型：

1. `lr_classifier.mlpackage`
2. `rf_classifier.mlpackage`
3. `xgb_classifier.mlpackage`

这三个模型现在都位于：

```text
swift-app/SleepRecovery/Sources/Resources/Models/
```

说明：

- `lr_classifier`：当前主部署模型
- `rf_classifier`：回退模型
- `xgb_classifier`：XGBoost 候选模型，已接入 Swift 端运行时兼容分支
- 仓库根目录下的 `models/xgb_classifier_v2.mlpackage` 仍保留为训练/export 产物，但 **当前 Swift App 并未直接加载它**

## 2. 这些模型对应加载什么 CSV

结论先说：

- **三个模型都不直接读取原始心率 CSV 或活动 CSV**
- **三个模型都只消费 `derived_features.csv` 中的同一组 9 维特征**

也就是说，`lr_classifier`、`rf_classifier`、`xgb_classifier` 对应的推理输入是同一类 CSV，不是三份不同格式的 CSV。

### 2.1 模型真实输入

Swift 端会从 `derived_features.csv` 解析出以下字段，再喂给 Core ML：

- `mean_hr`
- `min_hr`
- `max_hr`
- `hr_std`
- `hr_drop_first_90m`
- `activity_mean`
- `activity_spike_count`
- `sleep_fragmentation_score`
- `total_duration_min`

其中：

- `session_id` 用于会话定位和图表联动，不喂给模型
- `recovery_label` 是弱标签列，用于展示/回退，不是模型输入特征

### 2.2 各模型输出差异

虽然输入一致，但输出字段名略有差别：

- `lr_classifier` / `rf_classifier`：预测标签字段为 `classLabel`
- `xgb_classifier`：预测标签字段为 `recovery_label`

这也是 `PredictionService` 当前要兼容两种输出字段名的原因。

## 3. Swift App 当前能直接加载哪些 CSV

### 3.1 文件选择器直接支持

当前 UI 只支持用户主动选择：

- `derived_features.csv`

对应代码入口：

- `HomeView` 只在文件名包含 `derived_features` 或 `features` 时触发加载
- `FeaturePackageLoader` 只负责解析 `derived_features.csv` 的 schema

### 3.2 自动尝试加载的配套 CSV

在成功加载 `derived_features.csv` 后，Swift App 还会基于首个 `session_id` 自动尝试查找：

- `heart_rate_timeseries/<session_id>_hr.csv`
- `activity_timeseries/<session_id>_activity.csv`

它们的用途是：

- `heart_rate_timeseries/...`：给心率趋势图提供真实时序
- `activity_timeseries/...`：给活动强度图提供真实时序

注意：

- 这两个文件 **不是模型输入**
- 它们只影响图表，不影响 Core ML 推理

## 4. 为什么你现在感觉“似乎只能加载 derived_features.csv”

因为从当前实现上看，这个判断是对的：

- **推理入口** 确实只有 `derived_features.csv`
- 心率/活动 CSV 只是图表的可选配套数据，不会单独触发分析流程

另外，当前仓库里的现成数据目录和 Swift 端自动查找的目录结构并不完全一致：

- 当前仓库已有：
  - `data/features/derived_features.csv`
  - `data/samples/golden/heart_rate_timeseries.csv`
  - `data/samples/golden/activity_timeseries.csv`
- 但 Swift 端当前期待的是：
  - `<选中的 derived_features.csv 所在目录>/heart_rate_timeseries/<session_id>_hr.csv`
  - `<选中的 derived_features.csv 所在目录>/activity_timeseries/<session_id>_activity.csv`

因此：

- 你现在选择 `data/features/derived_features.csv` 时，可以正常跑推理
- 但图表大概率会回退到 placeholder 数据，因为同级目录下并没有符合命名规则的时序 sidecar 文件

## 5. 推荐使用方式

### 5.1 只验证模型推理

直接选择：

```text
data/features/derived_features.csv
```

这会完成：

- 特征包解析
- Core ML 模型加载
- 恢复等级预测
- 概率与影响因素展示

### 5.2 同时验证图表

除了 `derived_features.csv` 之外，还需要在同级目录准备：

```text
<base-dir>/
├── derived_features.csv
├── heart_rate_timeseries/
│   └── <session_id>_hr.csv
└── activity_timeseries/
    └── <session_id>_activity.csv
```

只有满足这个布局，Swift App 才能把真实时序图加载出来。

## 6. 对 README 使用者最重要的结论

- **三个已接入 App 的 Core ML 模型都共用同一个 `derived_features.csv`**
- **心率/活动时序 CSV 不是模型输入，而是图表 sidecar**
- **当前仓库现成数据足够验证推理，但不一定满足真实图表 sidecar 的目录布局**
- 如果后续要把 `xgb_classifier_v2` 作为主部署模型，需要同步更新 Swift 端的模型查找列表与 README/DEMO 文档
