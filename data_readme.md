# Sleep Recovery MVP 数据集选择建议

这份文档不再保留“所有候选数据集的大列表”，而是根据当前项目的 `spec` 和 `tickets`，直接给出更适合 `Sleep Recovery MVP` 的数据集选择建议。

当前项目对主数据集的要求是：

1. 尽量来自 `Kaggle`
2. 尽量贴近 `可穿戴设备导出格式`
3. 最好同时覆盖 `睡眠`、`活动`、`心率` 中的两类以上信息
4. 能支持后续的：
   - `C++` 数据清洗与标准化
   - `Python / Colab` 特征工程与训练
   - `Core ML` 推理验证
   - `SwiftUI` 端的趋势图展示

---

## 主推荐数据集

### 1. Bellabeat / Fitbit Fitness Tracker Data

- 链接：
  - `Bellabeat` Kaggle 版本：
    [https://www.kaggle.com/datasets/neetgill/bellabeat](https://www.kaggle.com/datasets/neetgill/bellabeat)
- 数据特点：
  - 包含 `daily activity`
  - 包含 `minute-level sleep`
  - 包含 `heart rate`
  - 还有 `weight / BMI` 等补充信息
  - 数据来自约 `30` 名 Fitbit 用户，时间跨度约 `2` 个月

### 为什么它最适合当前 MVP

这是当前最贴近我们项目目标的主数据集，原因如下：

1. 它不是单纯的“生活方式表格数据”，而是更接近真实可穿戴设备导出的多文件结构。
2. 它同时覆盖了：
   - 睡眠相关数据
   - 活动相关数据
   - 心率相关数据
3. 这正好匹配当前 `spec` 中的主要展示目标：
   - 单夜心率趋势图
   - 活动趋势图
   - 恢复等级
   - 概率分布
   - 多夜趋势
4. 它非常适合体现“真实工程问题处理能力”，因为它天然会带来：
   - 多文件合并
   - 时间粒度不一致
   - 用户 ID 对齐
   - 缺失值处理
   - 日级与分钟级数据融合
5. 即使它没有现成的 `recovery_label`，也非常适合按当前项目方案生成 `规则弱标签`。

### 它在项目中的角色

- `MVP 主训练数据集`
- `C++ 预处理主输入`
- `Swift 趋势图主展示数据`
- `弱标签生成` 的主要载体

### 预计会遇到的真实问题

这个数据集很适合做项目，但也不会太“干净”，正好符合我们当前想体现的工程分析能力：

1. 样本量不大，只有约 `30` 名用户
2. 时间跨度有限
3. 文件拆分较多，需要做整合
4. 没有天然的“恢复评分”标签
5. 需要自己定义夜间会话、聚合特征和弱标签策略

这些问题不是缺点，反而是这个 MVP 最值得做的部分。

---

## 次推荐数据集

### 2. Child Mind Institute - Detect Sleep States

- 链接：
  [https://www.kaggle.com/competitions/child-mind-institute-detect-sleep-states](https://www.kaggle.com/competitions/child-mind-institute-detect-sleep-states)
- 数据特点：
  - 腕戴式加速度计时序数据
  - 有 `sleep onset / wakeup` 标注
  - 更偏真实时序检测任务

### 为什么不是主推荐

这个数据集在“时序建模”和“睡眠事件检测”上非常强，但它不适合作为当前 MVP 的主数据集，原因是：

1. 它更偏 `事件检测`，而不是我们当前要做的 `睡眠恢复评估`
2. 它主要是 `加速度计` 数据，对 `心率趋势图` 支持不足
3. 数据格式更复杂，适配成本更高
4. 更适合放到中期，作为：
   - 更真实的时序增强方向
   - 纯活动信号的扩展实验

### 它更适合的阶段

- `中期增强`
- `纯时序检测实验`
- `活动信号特征工程升级`

---

## 备选合成数据集

### 3. Sleep Health & Daily Performance Dataset

- 链接：
  [https://www.kaggle.com/datasets/mohankrishnathalla/sleep-health-and-daily-performance-dataset](https://www.kaggle.com/datasets/mohankrishnathalla/sleep-health-and-daily-performance-dataset)
- 数据特点：
  - `100,000` 条记录
  - 纯 `CSV`
  - 字段多、样本大
  - 明显偏合成数据

### 为什么它适合作为备选而不是主线

它适合做：

1. 规则验证
2. 模型 warm-up
3. schema 设计演练
4. 弱标签逻辑演练

但不适合作为当前 MVP 主数据集，原因是：

1. 更偏表格数据，不够接近真实 wearable 导出结构
2. 不足以支撑我们想要的“心率/活动趋势图 + 多文件预处理”工程形态
3. 更像“快速建模数据”，不够像“端到端产品数据”

---

## 当前最终建议

### 推荐决策

当前项目建议采用下面这套策略：

1. `主数据集`：
   - `Bellabeat / Fitbit Fitness Tracker Data`
2. `次数据集`：
   - `Child Mind Institute - Detect Sleep States`
3. `合成兜底数据`：
   - `Sleep Health & Daily Performance Dataset`

### 具体落地方式

- `Bellabeat / Fitbit`：
  - 用作 MVP 主线
  - 解决真实的多文件、多粒度、弱标签问题
- `CMI Detect Sleep States`：
  - 暂不进入 MVP 主线
  - 保留为中期时序增强实验
- `Sleep Health & Daily Performance`：
  - 用作结构补齐、合成兜底或快速对照实验

---

## 与当前 Spec 的对齐

选择 `Bellabeat / Fitbit Fitness Tracker Data` 作为主数据集，和当前 `spec` 的一致性最高，因为它最容易支持以下目标：

1. `Kaggle` 公开数据起步
2. `C++` 进行数据适配和标准化
3. 使用 `规则弱标签` 生成 `recovery_label`
4. 生成 `feature package`
5. 支持 `SwiftUI` 展示：
   - 心率趋势图
   - 活动趋势图
   - 恢复等级
   - 概率分布
   - 多夜趋势
6. 支持 `XGBoost / LightGBM` 的结构化特征训练路线

---

## 下一步建议

如果按这份选择继续推进，下一步优先做的是：

1. 下载并检查 `Bellabeat / Fitbit` 数据文件结构
2. 明确哪些文件进入主链路
3. 设计 `night session` 聚合规则
4. 设计 `recovery_label` 的规则弱标签方案
5. 产出第一版 `artifact manifest`

这会直接对应到当前 ticket：

- `01-lock-data-contract-and-golden-sample`
- `02-build-single-night-preprocessing-to-feature-package`
