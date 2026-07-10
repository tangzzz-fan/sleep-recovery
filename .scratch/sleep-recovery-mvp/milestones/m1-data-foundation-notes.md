# M1: Data Foundation — 知识难点与解决方案

> 对应 Ticket 01。记录实施过程中遇到的关键决策、陷阱和解决方案，
> 为后续 milestone 和未来维护者提供上下文。

---

## 1. Fitbit 数据集 12 小时制时间戳 AM/PM 解析

**问题**：Fitbit 原始 CSV 使用 `M/D/YYYY H:MM:SS AM/PM` 格式（12 小时制）。
例如 `4/1/2016 1:00:00 AM`。

如果直接用 Python 的 `strptime` 不指定 `%I`（12 小时制），会把 AM/PM 搞混。
常见错误：用 `%H`（24 小时制）解析 12 小时制数据，导致下午 1 点（1:00 PM）被读成凌晨 1 点。

**解决方案**：
```python
# 正确方式
from datetime import datetime
dt = datetime.strptime("4/1/2016 1:00:00 AM", "%m/%d/%Y %I:%M:%S %p")
# → 2016-04-01 01:00:00

# 错误方式（会丢失 AM/PM 信息）
dt = datetime.strptime("4/1/2016 1:00:00 AM", "%m/%d/%Y %H:%M:%S %p")
# → ValueError
```

**关键洞察**：SQLite / CSV 都不原生支持时区。后续 C++ 预处理中，统一输出为 UTC ISO 8601 格式（`2016-04-01T01:00:00Z`），一劳永逸解决时区问题。

**反思**：Python `datetime` 是 naive 的（没有时区信息），ISO 8601 输出时加了 `Z`（表示 UTC），但实际源数据是本地时间（用户佩戴时区未知）。这引入了精度损失。当前决策：**假设所有 Fitbit 数据为 UTC**，因为：(a) 源数据没有时区字段，(b) 睡眠恢复评估对时间精度要求不高（分钟级粒度足够），(c) 12 个用户的样本量太小，无法统计校准时区偏差。

---

## 2. 睡眠窗口检测：AM/PM 跨越午夜

**问题**：用户 2347167796 在 4/1 的睡眠数据从 1:00 AM 开始，到 12:59 AM 结束。
这看起来像整整 24 小时，实际上是一段从凌晨 1 点到早上 6:30 的睡眠。

**原因**：Fitbit 的 `minuteSleep_merged.csv` 按日历日切分，每段睡眠都在同一日期下。
凌晨 1 点到 6:30 全部归属 4/1，后面的"12:00 AM → 12:59 AM"实际上是 4/2 的凌晨。

**解决方案**：在 `detect_sleep_window()` 中按 datetime 排序（而不是原始字符串排序），
找到最长的连续 sleep 段（gap > 60 min 视为断点）。

**实际效果**：
- 原始 391 条记录按正确时间排序后，从 `2016-04-01 00:00:00` 到 `2016-04-01 06:30:00`
- 391 分钟连续睡眠窗口，不是 24 小时
- 间歇性变化：2 次苏醒(val=3)、15 次 restless(val=2)

**参考**：`python/extract_golden_sample.py` 中的 `detect_sleep_window()` 函数

---

## 3. 心率数据秒级粒度 → 分钟级聚合

**问题**：`heartrate_seconds_merged.csv` 是秒级数据（每 5 秒一次采样），
而模式设计、睡眠记录和活动都是分钟级的。必须在预处理中做聚合。

**数据特点**：
- 有些分钟只有 1 个采样点，有些有 12-15 个
- 采样间隔不均匀（5-15 秒不规则间隔）
- 85,700 条秒级数据聚合到 355 个有效分钟

**解决方案**：
```python
def aggregate_hr_to_minute(hr_rows):
    minute_data = {}
    for dt, val in hr_rows:
        minute_key = dt.replace(second=0)
        minute_data.setdefault(minute_key, []).append(val)
    # 每分钟取 mean, count, quality_flag
```

**坑**：如果直接用 `floor(dt)` 或 `datetime.strip()`，会把秒级精度丢掉，
导致后续 HR 时间戳和睡眠窗口对齐出错。必须保留 `_dt` 字段在中间状态。

**C++ 关注点**：后续 M2 在 C++ 中实现时，会有相同问题。
秒级数据用 `std::unordered_map<time_t, std::vector<double>>` 聚合，
然后用 `std::accumulate` 计算 mean。

---

## 4. 弱标签权重初始值与实际分布的偏差

**问题**：设计 `data-contract.md` 的 weak-label formula 时，权重是手动估算的。
黄金样本用户 2347167796 的 score=95.8（几乎满分），
但这个用户本身就是数据中恢复最好的一个（391 min 深睡 + HR 60bpm + 几乎无活动）。

**潜在问题**：
- 12 个用户中可能大量分布在高分区（60-90 分），导致类别不均衡
- 5 个 penalty 的权重需要在实际数据上调试和校准

**解决方案**：
- **当前**（M1）：使用初始权重锁定 formula 作为 baseline
- **M2**（预处理跑通后）：对 12 个用户的全量数据跑一遍弱标签，
  统计 recovery_label 的分布。如果 80%+ 都是 label=2，需要调高 penalty 使分布更均衡
- **M3**（训练阶段）：对比弱标签和特征驱动的半监督方法，
  优先保证训练数据的标签质量

**记录在案**：`docs/data-contract.md` §3.3（Design Notes 和注意事项）

---

## 5. 黄金样本的"完美之夜"问题

**问题**：选择的黄金样本（用户 2347167796, 4/1）是一个"完美之夜"：
recovery_score=95.8，label=2 (Good)。这导致后续 M2/M3/M4 的验证测试**缺乏多样性**。

**隐患**：如果 C++/Python/Core ML/Swift 只在"完美之夜"上验证一致性，
可能无法发现模型在"差"或"一般"的夜间数据上的问题。

**缓解措施**：
- 在 M2 完成后，至少增加 2 个额外验证样本：1 个"差"的夜晚、1 个"一般"的夜晚
- 从 12 个用户中筛选 recovery_score < 50 和 50-75 范围的夜晚
- 黄金样本保留为 main reference，不删不改

---

## 6. Python 3.9 兼容性

**问题**：macOS 系统自带的 Python 是 3.9.6（Xcode 自带），
而很多现代 Python 库（numpy 2.x、pandas 3.x）需要 Python 3.10+。

**影响**：
- MVP 目前只用了 numpy 1.26.4（兼容 3.9）+ pandas 2.3.3（兼容 3.9）
- uv 自动限制了版本范围

**长期风险**：
- `coremltools`、`xgboost`、`lightgbm` 在 Python 3.9 上的版本受限
- Colab 上的 Python 版本可能和本地不一致

**缓解方案**：
- M3 阶段考虑用 `uv python install 3.11` 切换到更新的版本
- 或者在 Colab 上用相同的 Python 版本跑训练

---

## 7. `.gitignore` 调试：Python 文件的包含/排除冲突

**问题**：之前的 `.gitignore` 中写了 `python/*.pyc` 又写了 `!python/*.py`，
导致 git 无法正确识别 Python 源代码目录。

**根因**：gitignore 的排除规则优先级不是"后写的覆盖前面的"。
`*` 通配符规则和 `!` 否定规则之间没有层级关系，冲突时行为不可预测。

**修复**：删除所有 `!python/...` 规则，仅在 `python/.gitignore` 中排除 `.venv/` 和 `__pycache__/`。

**经验**：gitignore 应该用"白名单思维"（明确排除少数目录）而非"黑名单思维"（排除一切再包含少数）。

---

## 8. 数据契约文档的完整性

**关键决策**：创建 `docs/data-contract.md` 而非把 schema 嵌在 `spec-and-milestones.md` 中。

**原因**：
- M2（C++ 实现）需要精确的字段映射规则、编码格式、转换逻辑
- M3（Python 训练）需要精确的特征定义和标签范围
- M4（Swift 端）需要精确的输入输出命名
- 所有三个后续 milestone 都引用同一份数据契约，避免版本漂移

**未来注意**：如果 `data-contract.md` 在 M2/M3 阶段修改，必须更新 schema_version（`1.0.0` → `1.1.0`），确保 artifact manifest 和实际输出一致。

---

## 9. 后续 Milestone 的建议

基于 M1 的经验，M2/M3/M4 应该注意：

- **M2**：C++ 中处理 CSV 比 Python 复杂 10 倍。建议先写 Python 原型（data-contract.md 的 Python 实现已经做了 60%），然后在 C++ 中实现相同的逻辑。黄金样本作为验收标准。
- **M3**：coremltools 的 `convert()` 函数对 XGBoost/LightGBM 的支持取决于具体版本的 feature types。必须先在本地验证再在 Colab 上训练，以避免训练完才发现无法导出。
- **M4**：Swift 端加载 Core ML 模型需要精确匹配输入特征名称，命名规范必须和 Python 侧一致，否则 silent failure（返回错误的推理结果而不是报错）。
