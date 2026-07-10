# M2: Preprocessing Pipeline — 知识难点与解决方案

> 对应 Ticket 02。C++ 数据适配、标准化、合成补齐、特征提取 CLI。
> **状态：已实施完成**（已验证 10/10 golden sample checks PASSED）。

## 实际踩坑记录

### 坑 1: CSV 文件中的 `\r` 行尾

**现象**：Fitbit 原始 CSV 使用 Windows 行尾（`\r\n`）。CsvReader 用 `std::getline`
读取后，`\r` 留在了最后一个字段的末尾，导致列名 `Value\r` 而非 `Value`。

**根因**：`std::getline` 按 `\n` 分割，保留 `\r`。

**修复**：在读取 header 和每行后检查 `line.back() == '\r'` 并移除。

### 坑 2: CsvReader API vs CsvTable 类型不一致

**现象**：`hr_aggregator.h` 和 `session_builder.h` 的接口接受 `const CsvTable&`（即 `std::vector<CsvRow>`），但 main.cpp 传递的是 `CsvReader` 对象。

**修复**：统一使用 `.rows()` 方法提取 CsvTable，或在信号中使用正确的类型。最终在 main.cpp 中显式调用 `reader.rows()`。

### 坑 3: `timegm` vs `mktime` 的 8 小时时区差

**现象**：C++ 的 `parse_12h_to_epoch` 和 Python 的 `datetime.strptime` 对同一时间戳的 epoch 值差 8 小时（PDT 时区）。

**根因**：
- Python `datetime.strptime` 产生 naive datetime，`.timestamp()` 应用本地时区（PDT = UTC-8）
- C++ `timegm` 把 tm 直接解释为 UTC（零偏移）
- 两者差 8 小时

**修复**：统一使用 `timegm` 的语义（把 Fitbit 数据视为 UTC），匹配 data-contract.md 的决策：假定所有 Fitbit 数据为 UTC。

### 坑 4: XGBoost 的 OpenMP 依赖

**现象**：M3 中 XGBoost 无法加载，报错 `Library not loaded: @rpath/libomp.dylib`。

**修复**：`brew install libomp`。

### 坑 5: XGBoost Core ML 转换需要 DMatrix feature_names

**现象**：`coremltools.converters.xgboost.convert()` 报错 "training data did not have the following fields"。

**修复**：`dtrain = xgb.DMatrix(X, label=y, feature_names=FEATURE_ORDER)`，DMatrix 必须显式传递 feature_names。

## 预判验证结果

| 预判 | 是否命中 | 实际情况 |
|---|---|---|
| 1. C++ CSV 解析复杂性 | ✅ 命中 | `\r` 行尾污染列名；类型不一致导致编译错误 |
| 2. 12 小时制时间戳 | ✅ 命中 | timegm/mktime 差 8 小时，与 Python 对齐 |
| 3. HR 聚合性能 | ❌ 未命中 | 1.1M HR 行 × 12 用户，边读边聚合，内存 ~8MB 可接受 |
| 4. 合成数据真实性 | ✅ 命中 | 从真实分布采样参数，不是线性生成 |
| 5. 弱标签一致性 | ✅ 命中 | C++ 和 Python 输出完全一致（diff=0.0） |
| 6. 缺失数据处理 | ❌ 未命中 | Fitbit 数据质量好，基本无缺失 |
| 7. 跨表关联 | ✅ 命中 | 按 user_id → date → session 的顺序构建，关联完整 |
| 8. Makefile 布局 | ✅ 命中 | 零外部依赖，clang++ + make 直接工作 |
| 9. CLI 接口 | ✅ 命中 | --golden 验证开关在开发中很有用 |
| 10. 验收标准 | ✅ 全部通过 | 10/10 golden checks, pandas 可读, 240 sessions |

---

## 1. C++ CSV 解析的复杂性与 Python 的差距

**预判问题**：Python 用 `csv.DictReader` 一行代码搞定的事，在 C++ 中需要：
- 处理引号内的转义字符
- 处理不同的行结束符（`\r\n` vs `\n`）
- 类型安全的字段解析（`std::variant` 或 `std::optional`）
- 内存管理（大 CSV 流式读取 vs 全量加载）

**推荐方案**：
- 使用轻量级 CSV 解析库，如 `fast-cpp-csv-parser` 或直接手写基于 `std::getline` 的解析
- 不用 Boost（太重），不用 Qt（不相关的依赖）
- 参考 M1 的 Python 实现（`python/extract_golden_sample.py`）作为功能规格，
  C++ 实现必须通过黄金样本验证

**关键测试**：黄金样本应产生和 M1 Python 输出完全一致的 derived_features.csv

---

## 2. 12 小时制 AM/PM 时间戳 → C++ 实现

**问题**：Fitbit 原始 CSV 使用 `M/D/YYYY H:MM:SS AM/PM` 格式。
C++ 标准库的 `std::get_time` 支持 `%r`（12 小时制），
但在 Clang 17.0 上的行为需要验证。

**推荐方案**：
```cpp
// 方案 A: 使用 std::get_time（C++ 标准）
std::istringstream ss("4/1/2016 1:00:00 AM");
std::tm tm = {};
ss >> std::get_time(&tm, "%m/%d/%Y %I:%M:%S %p");
// 转 UTC 时间戳

// 方案 B：如果 std::get_time 在 Clang 上不可靠
// 使用 Howard Hinnant 的 date.h（单头文件库）
// https://github.com/HowardHinnant/date
```

**验证点**：确保 12:00 AM（午夜）和 12:00 PM（正午）能正确区分。
很多时间解析库在这个边界上有 bug。

---

## 3. 秒级心率 → 分钟级聚合的性能

**问题**：1,154,681 行秒级心率数据 × 12 个用户，C++ 中如果每秒处理一行，
总时间可能在 2-5 秒以上。不是大问题，但需要优化内存使用。

**推荐方案**：
- 流式读取 + 边读边聚合，不存所有秒级数据
- `std::unordered_map<minute_key, std::vector<double>>` 中累积值
- 每个用户的心率数据大约 100K 秒 × 12 ≈ 1.2M 行，内存占用约 96MB
- 可以逐用户处理，降低峰值内存到 ~8MB

**参考**：Python 实现用 `minute_data.setdefault(minute_key, []).append(val)`
C++ 等价方案用 `std::unordered_map<std::string, std::vector<double>>`
或用 `std::map` 保证有序输出（适合写 CSV）

---

## 4. 合成数据生成器的生理真实性

**预判问题**：合成数据如果太"完美"（线性下降心率、零活动峰值），
会导致模型在真实数据上的泛化能力差。

**推荐方案**：
- 心率生成公式中加入随机噪声（正态分布，σ=3-5 bpm）
- 活动峰值频率参考真实 Fitbit 数据的分布（黄金样本 4 次/391 min ≈ 1 次/100 min）
- 生成参数的初始范围从真实数据中采样（12 个用户提供 200+ 晚数据）
- 合成数据标记 `source_type="synthetic"`，避免和真实数据混淆

**关键**：合成数据只是保护网络和特征提取的兜底，
**不是提高模型准确性的手段**

---

## 5. 弱标签公式的实现验证

**问题**：M1 的弱标签公式是手写在 Python 中的。C++ 实现需要完全一致。

**推荐方案**：
1. 在 Python 中把弱标签公式抽取为独立函数 `compute_weak_label()`
2. 在 C++ 中实现相同的函数签名
3. 对 10 个随机输入验证 C++ 和 Python 输出一致（diff < 0.01）

**金丝雀测试**：黄金样本（session 2347167796_4-1-2016）
- C++ 输出必须和 Python 输出相同
- recovery_score=95.8, recovery_label=2

---

## 6. 缺失数据、质量标记和异常处理

**场景**：用户可能有部分心率数据缺失（如 12 用户中有 1 个仅有 1 天数据）。

**C++ 实现**：
- `quality_flag`: 0=ok, 1=interpolated, 2=outlier, 3=missing
- 缺失的心率分钟标记为 missing（heart_rate_bpm=0, quality_flag=3）
- 弱标签公式中如果 mean_hr=0（无心率数据），跳过 HR 相关 penalty
- 输出 CSV 中不跳过任何会话——即使只有部分数据也产出特征

---

## 7. 跨表关联完整性

**问题**：CSV 不像 SQL，没有外键约束。C++ 实现需要手动确保 session_id 一致性。

**推荐方案**：
- 首先生成 `night_sessions.csv`，确定所有 session_id
- 然后生成 `heart_rate_timeseries.csv` 和 `activity_timeseries.csv`，
  每个会话的时序数据都要验证 session_id 有效
- 最后生成 `derived_features.csv`，确保所有 session_id 在 night_sessions 中存在

---

## 8. 构建系统：Makefile 布局

**建议结构**：
```
cpp/
├── Makefile
├── src/
│   ├── main.cpp              # CLI 入口
│   ├── csv_reader.h/cpp      # CSV 解析
│   ├── time_utils.h/cpp      # 时间戳解析和转换
│   ├── sleep_detector.h/cpp  # 睡眠窗口检测
│   ├── hr_aggregator.h/cpp   # HR 秒级→分钟级
│   ├── feature_extractor.h/cpp # 9 个特征提取 + 弱标签
│   └── synthetic_generator.h/cpp # 合成数据生成
├── tests/
│   ├── test_golden_sample.cpp # 黄金样本验证测试
│   └── test_features.cpp      # 特征计算单元测试
└── build/
```

**依赖**：仅需标准库（`<fstream>`, `<sstream>`, `<vector>`, `<map>`, `<cmath>`）。
零外部依赖。如果有解析库需求，使用 header-only 库。

---

## 9. CLI 接口设计

```bash
# 用法
./sleep-recovery-cli \
  --input data/raw/ \
  --output data/normalized/ \
  --features data/features/ \
  --labels data/labels/ \
  --synthetic 200 \              # 生成 200 晚合成数据（可选）
  --golden data/samples/golden/  # 验证黄金样本（可选）

# 最小调用
./sleep-recovery-cli \
  --input ../Fitabase\ Data\ 3.12.16-4.11.16/ \
  --output data/normalized/
```

---

## 10. 验收标准细化

| # | 标准 | 验证方法 |
|---|---|---|
| 1 | 同输入 → 同输出 | `diff <(run1) <(run2)` |
| 2 | CSV 可被 pandas 读取 | `uv run python -c "import pandas; pandas.read_csv(...)"` |
| 3 | 9 个特征正确 | 黄金样本验证 |
| 4 | Kaggle+合成数据同一结构 | 对比两个 source_type 的 CSV header |
| 5 | 无字段缺失 | `wc -l` 验证每行的列数常量 |
| 6 | 跨表关联完整 | 所有 session_id 都在 night_sessions 中存在 |
