# M4: macOS Deployment & Delivery — 知识难点与解决方案

> 对应 Tickets 05 + 06 + 07。macOS SwiftUI 应用，Core ML 集成，全链路验证，交付文档。
> **状态：已实施完成**（SwiftUI app 编译启动正常，多夜趋势图集成，全部 7 个 tickets 交付）。

## 实际踩坑记录

### 坑 1: Swift Charts 需要显式 import

**现象**：`Chart`、`LineMark`、`BarMark`、`RuleMark` 找不到。
**修复**：HomeView.swift 顶部添加 `import Charts`。

### 坑 2: Swift Package Manager 源码路径配置

**现象**：SPM 找不到源文件（"target is empty"），因为源码在非标准路径。
**修复**：Package.swift 中显式设置 `path: "SleepRecovery/Sources"`。

### 坑 3: Core ML 输出概率字典的 key 类型转换

**现象**：`output.featureValue(for: "classProbability")?.dictionaryValue` 的 key 是 `AnyHashable`，不能直接传递给 `Int()`。
**修复**：`Int("\(key)")`（先转字符串再转整数）。

### 坑 4: 跨层一致性的 silent failure 风险

**现象**：如果 Swift 端输入特征名与 Core ML 模型不一致，不会报错但结果错误。
**验证**：黄金样本在 C++→Python→CoreML→Swift 四层上做了显式对比，所有层级都通过。

### 坑 5: `#Preview` 宏在 macOS 14.0 目标上可用

**现象**：`#Preview` 宏是 Swift 6 / macOS 14+ 的新特性，在旧版本会报错。
**确认**：Package.swift 设置了 `.macOS(.v14)`，所以没问题。

### 坑 6: 重复 `#Preview` 导致编译失败

**现象**：编辑 HomeView 时不小心留下了两个 `#Preview` 块，导致宏展开错误。
**修复**：删除重复的 `#Preview`，保留一个。

## 预判验证结果

| 预判 | 是否命中 | 实际情况 |
|---|---|---|
| 1. Core ML 输入类型匹配 | ✅ 命中 | float32/double 自动转换，独立特征名输入方式正确 |
| 2. Swift Charts 心率图 | ✅ 命中 | LineMark + RuleMark + AxisMarks 按预期工作 |
| 3. 多夜趋势状态管理 | ✅ 命中 | TrendView 独立管理 [NightResult]，allSessions 共享 |
| 4. 影响因素展示 | ⚠️ 简化 | 采用规则判断（方案 B）而非模型特征重要性 |
| 5. macOS 版本/签名 | ✅ 命中 | macOS 14.0, 本地开发无需签名 |
| 6. 全链路验证 | ✅ 命中 | 四层验证全部通过 |
| 7. Core ML 优化 | ✅ 完成 | coreml-optimization.md 已有详细分析 |
| 8. DEMO 文档 | ✅ 完成 | DEMO.md 含快速开始指南 |
| 9. 文件选择器 | ✅ 命中 | fileImporter 正常工作 |
| 10. 交付文档清单 | ✅ 完成 | 7 份文档全部产出 |

---

## 1. Core ML 模型加载和输入类型匹配

**最大坑**：Core ML 的 MLMultiArray 输入类型必须和 coremltools 导出的完全一致。

**具体问题**：
- `coremltools` 默认输出 `float64`（Double），但 Swift 端 Core ML 偏好 `float32`
- 转换时需要在 `coremltools.convert()` 中指定 `minimum_deployment_target`
  并在转换参数中设置 `float32`
- 如果类型不匹配，Swift 侧会抛出运行时错误，不给具体原因

**推荐方案**：
```python
# Python 训练后导出（M3）
import coremltools as ct
model = ct.convert(xgb_model,
    inputs=[ct.TensorType(shape=(1, 9), dtype=np.float32)],
    outputs=[ct.TensorType(dtype=np.float32)],
    minimum_deployment_target=ct.target.macOS14)
model.save("sleep_recovery.mlpackage")
```

```swift
// Swift 端加载（M4）
guard let model = try? SleepRecoveryModel(configuration: .init()) else { ... }
let input = try MLMultiArray(shape: [1, 9], dataType: .float32)
// 按模型适配层规定的特征顺序填充
let output = try model.prediction(input: SleepRecoveryModelInput(features: input))
```

**验证**：黄金样本的 Python 输出和 Swift 推理结果必须在每类概率上 < 0.01 diff。

---

## 2. Swift Charts 的心率趋势图渲染

**技术要求**：
- 展示整夜心率的时间序列（秒级或分钟级聚合后的折线图）
- 横轴：时间（UTC → 本地时间可读格式）
- 纵轴：bpm 范围（建议固定 y 轴 40-120 bpm，可比性更好）

**推荐实现**：
```swift
Chart {
    ForEach(heartRateData) { point in
        LineMark(
            x: .value("Time", point.timestamp),
            y: .value("HR", point.heartRate)
        )
    }
    RuleMark(y: .value("Resting", 70))
        .foregroundStyle(.green.opacity(0.3))
}
.chartYScale(domain: 40...120)
```

**注意**：时间戳从 ISO 8601 字符串转为 `Date` 类型，Swift 的 ISO8601DateFormatter
默认期待带 `Z` 后缀的 UTC 时间。M2 产出的 CSV 应该已经包含 `Z`。

---

## 3. 多夜趋势视图的状态管理

**问题**：单夜视图（Ticket 05）实现后，多夜视图（Ticket 06）需要加载多个特征包。

**推荐数据流**：
```
FeaturePackageLoader
  → 读取多个 derived_features.csv
  → 合并为 [NightSession]
  → 传给 TrendViewModel
  → TrendView 渲染折线图
```

**状态模型**：
```swift
class TrendViewModel: ObservableObject {
    @Published var sessions: [NightSession] = []
    @Published var selectedDateRange: DateInterval

    var recoveryTrend: [RecoveryPoint] {
        sessions.map { RecoveryPoint(date: $0.date, label: $0.label, score: $0.score) }
    }
}
```

**关键**：不要在两处重复加载 CSV 解析逻辑。单夜和多夜共用一个 `CSVLoader` 模块。

---

## 4. 关键影响因素的来源和展示

**实现方案（两选一）**：

**方案 A：基于模型特征重要性（如果 XGBoost/LightGBM 成功导出）**
- M3 训练阶段计算每个特征的重要性（gain 或 weight）
- 导出到元数据 JSON，Swift 端加载
- 对单次预测，展示 top 3 重要特征及对应值

**方案 B：离线预计算（如果模型不支持特征贡献）**
- M3 在所有 12 个用户的样本上计算全局 top 3 特征
- Swift 端只需展示固定列表 + 当次的特征值

**推荐**：初始用方案 B（简单可靠），后续用方案 A 增强（如果模型支持）。

---

## 5. macOS 部署的最低版本和签名

**配置**：
```
Deployment Target: macOS 14.0 (Sonoma)
Swift: 6.1
Xcode: 16.4
Core ML: 6.0+
Swift Charts: 系统框架（macOS 13+）
```

**签名**：
- 开发阶段用 `Development Team: -`（跳过签名，仅本地运行）
- MVP 不需要 App Store 分发，不需要 Notarization
- 但需要禁用 Sandbox（本地文件读取权限）

**Entitlements**：
```xml
<key>com.apple.security.app-sandbox</key>
<false/>
<key>com.apple.security.files.user-selected.read-only</key>
<true/>
```

---

## 6. 全链路一致性验证（Ticket 07）

**验证矩阵**：

| 验证 | 输入 | 预期 | 方法 |
|---|---|---|---|
| C++ → Python | golden derived_features.csv | 相同的特征值（diff < 0.01）| `diff` 命令 |
| Python → Core ML | golden feature vector | 相同的概率输出（diff < 0.001）| Python 调用 coremltools |
| Core ML → Swift | golden feature vector | 相同的概率输出（diff < 0.001）| Swift 调用 Core ML 推理 |
| Swift UI | golden feature package | 界面显示 recovery_label=2, score≈95.8 | 屏幕截图对比 |

**验收**：四层全部通过 golden sample 验证后才算 M4 完成。

---

## 7. Core ML 优化分析的内容要求

交付文档中需要包含：

1. **输入类型分析**：为什么选 float32 而不是 float64，对推理精度的影���
2. **输出结构适配**：三分类概率数组的解释方式
3. **量化可行性**：是否可以降低模型精度来减少模型大小（目前 ~10KB，不需要量化）
4. **部署复杂度对比**：XGBoost vs Random Forest vs Logistic Regression 的 Core ML 集成难度
5. **推理速度**：单次推理耗时（预期 < 1ms）

---

## 8. 项目演示的可分享性

**目标**：使最终交付的 macOS 应用可以被他人下载和运行。

**推荐方案**：
- 用 Xcode Archive → Export 生成 .app bundle
- 打包黄金样本数据一起分发
- 写一份 `DEMO.md`：如何打开应用、如何加载测试数据、如何理解展示结果

**不需要**：
- App Store 上架
- 代码签名（本地运行即可）

---

## 9. 文件选择器和数据加载 UI

**用户体验**：
- 应用启动后显示"加载数据"按钮
- 用户选择包含 CSV 文件的文件夹
- 自动识别 file layout（night_sessions.csv + 时序 CSVs）
- 加载成功后进入分析页

**Swift 实现**：
```swift
// 使用 NSOpenPanel 选择文件夹
let panel = NSOpenPanel()
panel.canChooseDirectories = true
panel.canChooseFiles = false
if panel.runModal() == .OK {
    let url = panel.url!
    viewModel.loadData(from: url)
}
```

---

## 10. 文档完整性清单（Ticket 07）

| 文档 | 内容 |
|---|---|
| `README.md` 更新 | 反映 MVP 完成状态 |
| `DEMO.md` | 如何运行和演示 |
| `spec-and-milestones.md` 更新 | 反映最终交付状态 |
| `data-contract.md` 更新 | 如有变更 |
| Core ML 优化分析 | 独立的分析文档 |
| 模型选择与对比分析 | 模型取舍的工程权衡 |
| 一致性验证报告 | 四层验证结果 |
