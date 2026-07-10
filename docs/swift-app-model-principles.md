# Swift App 模型原理说明

## 1. 这份文档解决什么问题

这份文档的目标不是教你从零开始学习机器学习，而是帮助你在阅读和维护当前项目时，能够回答下面几个实际问题：

- `LR`、`RF`、`XGB` 分别是什么
- 为什么项目里会同时保留 3 个模型
- 为什么 Swift App 看起来“只加载了一个 CSV”
- `derived_features.csv` 和 `heart_rate_timeseries.csv` / `activity_timeseries.csv` 的职责有什么区别
- 为什么模型要先转成 `Core ML`，再被 Swift 代码加载
- 为什么 `.mlpackage` 还要在运行时编译成 `.mlmodelc`
- 当前 Swift 端真正做了哪些事，哪些事没有做

如果你只关心一句话版：

- **这个项目不是让 Swift 直接分析原始睡眠数据**
- **Swift 只是读取已经算好的特征包，然后调用 Core ML 做一次分类预测**

---

## 2. 先从整体链路理解

当前项目的完整链路是：

```text
Fitbit 原始数据
    ↓
C++ 预处理
    ↓
derived_features.csv
    ↓
Python 训练 LR / RF / XGB
    ↓
导出为 Core ML .mlpackage
    ↓
Swift App 加载模型
    ↓
输入一行特征，输出 recovery 预测结果
```

把它拆开理解：

### 2.1 C++ 阶段做什么

C++ 负责把原始睡眠、心率、活动数据整理成“单晚会话”的结构化结果，并计算出模型真正需要的特征值。

它输出的核心结果是：

- `derived_features.csv`

这份 CSV 是已经提炼好的“特征包”，相当于把原始数据压缩成模型能直接吃的一组数字。

### 2.2 Python 阶段做什么

Python 不直接服务 App，而是负责离线训练模型。

它会读取：

- `derived_features.csv`

然后训练多个分类模型：

- `Logistic Regression`
- `Random Forest`
- `XGBoost`

最后把模型导出成 Apple 生态可用的 `Core ML` 格式。

### 2.3 Swift 阶段做什么

Swift App 不再自己做训练，也不自己从原始 Fitbit 数据算特征。

Swift 端只做三件事：

1. 读取 `derived_features.csv`
2. 把其中一行转换为模型输入
3. 用 Core ML 模型得到恢复等级预测，并展示结果

所以 Swift 端更像是一个：

- **本地推理客户端**

而不是：

- **训练平台**
- **特征工程平台**
- **原始数据分析器**

---

## 3. 什么是 `derived_features.csv`

这份 CSV 是整个 Swift 端推理链路的核心输入。

它里面一行就代表一个 night session，也就是“一晚睡眠会话”的摘要特征。

当前项目里，模型真正使用的特征共有 9 个：

- `mean_hr`
- `min_hr`
- `max_hr`
- `hr_std`
- `hr_drop_first_90m`
- `activity_mean`
- `activity_spike_count`
- `sleep_fragmentation_score`
- `total_duration_min`

此外还有两个很关键但职责不同的字段：

- `session_id`
- `recovery_label`

它们的区别是：

- `session_id`：用于标识是哪一晚的数据，便于和图表、时序数据关联
- `recovery_label`：弱标签，主要来自规则推导，是训练时的目标值，也可作为展示或模型缺失时的回退值

换句话说：

- **模型输入** 是 9 个数值特征
- **不是把整行所有字段都喂给模型**

---

## 4. 为什么 Swift 端看起来只能加载 `derived_features.csv`

这是设计使然，不是 bug。

### 4.1 因为模型需要的是“特征”，不是原始时序

原始睡眠数据本质上是时间序列，比如：

- 某一分钟心率是多少
- 某一分钟活动强度是多少

这种数据对训练来说当然有价值，但当前 MVP 不是让 Swift 在端上做时序特征提取。

当前方案选择的是：

- 先在 C++ 阶段把时序压缩成一组稳定、低维、可解释的特征
- 再让 Swift 只消费特征结果

这样做有几个直接好处：

- 端上逻辑更简单
- 推理速度更稳定
- 特征口径统一，不会出现 Python 和 Swift 算法不一致
- 更容易做跨层一致性验证

### 4.2 时序 CSV 在当前 Swift App 里的作用

当前项目里还有两类时序相关 CSV：

- `heart_rate_timeseries`
- `activity_timeseries`

它们在 Swift App 里的职责是：

- 给图表提供展示数据

它们**不是模型输入**。

所以你看到的真实情况是：

- `derived_features.csv` 决定模型预测结果
- `heart_rate_timeseries` / `activity_timeseries` 决定图表能不能画出真实曲线

---

## 5. `LR`、`RF`、`XGB` 到底是什么

这三个都是分类模型。

当前项目里，它们做的是同一件事：

- 根据一晚睡眠的 9 个特征，预测恢复等级是 `0 / 1 / 2`

区别不在“做不做分类”，而在“怎么做分类”。

### 5.1 LR：Logistic Regression

`LR` 的全称是 `Logistic Regression`，中文一般叫“逻辑回归”。

名字里虽然带 `Regression`，但它在这里是个**分类模型**。

你可以把它理解成：

- 给每个特征一个权重
- 把这些特征按权重加总
- 再把结果转换成每个类别的概率

它的特点是：

- **结构简单**
- **可解释性强**
- **训练和推理都比较快**
- 在样本量不大时，往往是很稳的 baseline

在这个项目里，`LR` 很适合作为：

- 主部署模型

因为它通常更稳定、容易理解、部署成本低。

### 5.2 RF：Random Forest

`RF` 的全称是 `Random Forest`，中文叫“随机森林”。

你可以把它理解成：

- 不是训练一棵决策树
- 而是训练很多棵树
- 每棵树各自给一个判断
- 最后投票或平均得出结果

它的特点是：

- **比单棵树更稳**
- **可以学习非线性关系**
- 对特征缩放通常不敏感
- 在结构化表格数据上经常表现不错

它适合作为：

- 回退模型

因为它和 `LR` 的建模思路很不同，可以在主模型表现不理想时提供另一条路线。

### 5.3 XGB：XGBoost

`XGB` 一般指 `XGBoost`。

它本质上也是基于树模型，但不是“很多树一起投票”的随机森林思路，而是：

- 一棵树一棵树地迭代训练
- 每一棵新树都尽量修正前面模型犯过的错误

你可以把它理解成：

- 模型在不断“补错题”

它的特点是：

- **对表格数据通常很强**
- **能拟合更复杂的非线性关系**
- 调参空间更大
- 工程复杂度和解释成本也更高

在这个项目里，`XGB` 是：

- 候选高性能模型

但它在工程上往往没有 `LR` 那么“轻”，所以项目里会同时保留多种模型。

---

## 6. 为什么要同时保留 3 个模型

这不是冗余，而是工程上常见的多模型策略。

### 6.1 原因一：对比效果

同样的特征，不同模型家族表现可能差很多。

保留多个模型，能回答：

- 这套特征到底适合线性模型，还是树模型
- 提升来自特征本身，还是来自模型复杂度

### 6.2 原因二：兼顾性能与可维护性

实际项目里很少只看“离线指标最高”。

还要同时考虑：

- 是否容易导出到 Core ML
- 端上推理是否稳定
- 是否好排查问题
- 是否容易解释结果

一个模型可能：

- 指标最好，但工程维护复杂

另一个模型可能：

- 指标略低，但部署更稳、解释更强

所以项目里常会有：

- 主部署模型
- 回退模型
- 未来候选模型

### 6.3 原因三：给 Swift 端保留运行时切换空间

Swift 端当前按顺序查找：

1. `lr_classifier`
2. `rf_classifier`
3. `xgb_classifier`

这意味着即使某个模型资源不存在，仍有机会继续落到后续模型上，不至于整个推理链路直接不可用。

---

## 7. 为什么这 3 个模型能共用同一个 CSV

因为它们虽然算法不同，但当前训练时都使用了同一套特征列。

也就是说，三者的输入 schema 是一致的：

- 都需要同一组 9 维特征

这对工程实现非常重要，因为这样 Swift 端只需要维护：

- 一套 `FeaturePackage`
- 一套 CSV 解析逻辑
- 一套模型输入字典构造逻辑

如果每个模型都要求不同输入格式，Swift 端就会复杂很多。

---

## 8. 为什么 `LR / RF` 和 `XGB` 的输出字段名不一样

这是模型导出链路的细节差异，不是 Swift 写错了。

当前项目里：

- `LR / RF` 导出后使用 `classLabel`
- `XGB` 导出后使用 `recovery_label`

因此 `PredictionService` 在读取输出时，必须兼容两种情况：

1. 先尝试读取 `classLabel`
2. 如果没有，再尝试读取 `recovery_label`

这也是为什么代码里会看到两个分支。

从工程角度讲，这种“适配层”很正常，它的目的就是：

- 屏蔽模型导出差异
- 保持上层 UI 不需要知道底层模型细节

---

## 9. 为什么要导出成 Core ML

因为当前 App 运行在 Apple 生态里，Swift 端最自然的本地推理方案就是 `Core ML`。

把 Python 里训练好的模型导出成 Core ML，有几个好处：

- Swift 可以本地加载，不依赖服务端
- 性能更稳定
- 更适合 Apple 平台部署
- 能把训练和端上推理解耦

这意味着：

- Python 负责“怎么学出来”
- Swift 负责“怎么在设备上跑起来”

这是一个典型的离线训练 + 端上推理结构。

---

## 10. `.mlpackage` 和 `.mlmodelc` 是什么关系

这两个名词很容易混淆。

### 10.1 `.mlpackage`

`.mlpackage` 可以理解成：

- 模型的打包目录

里面包含：

- 模型定义
- 元数据
- 描述文件

它更像“可分发的模型包”。

### 10.2 `.mlmodelc`

`.mlmodelc` 可以理解成：

- 经过编译后的可加载模型目录

它更接近真正要被运行时执行的格式。

### 10.3 当前项目为什么要在运行时编译

当前 Swift App 把模型资源以 `.mlpackage` 形式放进 `Resources/Models`，这样做的好处是：

- 不依赖 Xcode 的隐式 Core ML codegen
- SwiftPM 资源管理更明确
- 多模型资源更容易统一处理

但运行时要真正加载模型，还要走：

1. 找到 `.mlpackage`
2. 调用 `MLModel.compileModel(at:)`
3. 得到 `.mlmodelc`
4. 再用 `MLModel(contentsOf:)` 打开

这就是当前 `PredictionService` 的核心加载方式。

---

## 11. Swift 端当前的实现原理

### 11.1 模型加载阶段

Swift 启动后，`PredictionService` 会：

1. 在 `Bundle.module` 里找 `Models/`
2. 按顺序尝试 `lr_classifier`、`rf_classifier`、`xgb_classifier`
3. 找到 `.mlpackage` 后先编译成 `.mlmodelc`
4. 编译成功后创建 `MLModel`

这个过程的目标是：

- 不把模型当源码处理
- 不依赖自动生成的强类型包装类
- 保持运行时动态加载

### 11.2 预测阶段

当用户加载 `derived_features.csv` 后：

1. `FeaturePackageLoader` 读取 CSV
2. 解析每一行为 `FeaturePackage`
3. `PredictionService` 从 `FeaturePackage` 取出 9 个特征
4. 组装成 `[String: Any]`
5. 交给 `MLDictionaryFeatureProvider`
6. 调用 `model.prediction(from:)`
7. 解析标签和概率输出

也就是说，当前推理不是：

- 传原始文件给模型

而是：

- 先把文件读成结构体
- 再把结构体字段映射到模型需要的输入 key

### 11.3 图表阶段

图表数据和模型预测不是同一条链路。

模型预测依赖：

- `derived_features.csv`

图表依赖：

- `heart_rate_timeseries/...`
- `activity_timeseries/...`

如果图表 sidecar 文件缺失，模型预测依然能工作，只是图表会退回 placeholder。

---

## 12. 为什么这种架构适合当前 MVP

从工程角度看，这套设计很适合 MVP 阶段。

### 12.1 优点

- **职责清晰**：C++ 做特征，Python 做训练，Swift 做展示和推理
- **便于验证**：可以逐层核对 C++、Python、Core ML、Swift 是否一致
- **端上简单**：Swift 不需要实现复杂时序特征工程
- **部署稳定**：表格特征 + Core ML 的链路相对容易调试

### 12.2 限制

- Swift 端不能直接从原始 Fitbit 数据开始推理
- 图表目录结构目前和仓库现成数据布局不完全一致
- 多模型虽然都已打包，但当前默认仍优先使用 `lr_classifier`
- `xgb_classifier_v2` 还没有纳入 Swift 端的资源加载列表

---

## 13. 你在维护这部分代码时，最该记住什么

### 13.1 先记住职责边界

- **C++**：原始数据 → 特征
- **Python**：特征 → 训练好的模型
- **Swift**：特征 → 本地预测结果

### 13.2 先记住输入输出关系

- **模型输入**：`derived_features.csv`
- **图表输入**：时序 sidecar CSV
- **模型资源**：`swift-app/SleepRecovery/Sources/Resources/Models/*.mlpackage`

### 13.3 先记住三种模型的定位

- `LR`：稳、简单、易解释
- `RF`：树模型回退方案
- `XGB`：更强但更复杂的候选方案

### 13.4 先记住当前不是端上训练

Swift 端没有做这些事：

- 训练模型
- 调参
- 端上特征提取
- 原始 Fitbit 解析

Swift 当前做的是：

- **本地加载模型并执行一次分类预测**

---

## 14. 建议的阅读顺序

如果你想把这块真正读懂，建议按这个顺序看：

1. `docs/swift-app-model-usage.md`
   先理解“怎么用”
2. `docs/data-contract.md`
   理解 `derived_features.csv` 的字段定义
3. `python/train_models.py`
   理解模型训练到底喂了哪些列
4. `swift-app/.../FeaturePackageLoader.swift`
   看 Swift 如何解析 CSV
5. `swift-app/.../PredictionService.swift`
   看模型如何加载、如何预测

这样你会先有业务图景，再下钻到实现细节，不容易在一开始被 `LR / RF / XGB` 这些术语卡住。
