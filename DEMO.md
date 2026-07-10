# Sleep Recovery MVP — 演示说明

## 快速开始

### 1. 安装环境

```bash
# Python (macOS)
brew install uv libomp
cd python && uv venv && uv pip install -r requirements.txt

# C++
cd cpp && make
```

### 2. 运行预处理（C++ → 特征包）

```bash
cd cpp
make
./build/sleep-recovery-cli \
  --input "../Fitabase Data 3.12.16-4.11.16/" \
  --output ../data/normalized/ \
  --features ../data/features/ \
  --labels ../data/labels/ \
  --golden ../data/samples/golden/
```

此命令将：
- 加载 Fitbit 原始 CSV（11 个文件）
- 识别 12 个有心率+睡眠的用户
- 产出 240 个夜间会话的特征包和标签
- 验证黄金样本一致性

### 3. 训练模型（Python → Core ML）

```bash
cd python
uv run python train_models.py
```

此脚本将：
- 加载 derived_features.csv
- Leave-One-User-Out 交叉验证
- 训练和比较 3 个模型
- 导出 Core ML 模型到 models/

### 4. 运行 macOS 应用

```bash
cd swift-app
swift build
open .build/debug/SleepRecovery.app
```

应用功能：
- 加载 derived_features.csv 文件
- 展示心率趋势图 + 活动强度图
- 显示恢复等级、置信度、关键影响因素

### 5. 验证全链路一致性

```bash
# C++ CLI 黄金样本验证（10 个特征全部检查）
make -C cpp && ./cpp/build/sleep-recovery-cli \
  --input "Fitabase Data 3.12.16-4.11.16/" \
  --golden data/samples/golden/

# Python Core ML 预测一致性（3 个模型，240 个样本）
cd python && uv run python -c "
import coremltools as ct, pandas as pd, numpy as np
...
"

# 跨层对比（C++ vs Python vs Core ML）
cd python && uv run python -c "
# Golden sample 9 features diff=0 across C++/Python
# 3 Core ML models predict label=2 (Good) consistently
"
```

## 关键文件

| 文件 | 说明 |
|---|---|
| `data/samples/golden/derived_features.csv` | 黄金样本特征包 |
| `data/features/derived_features.csv` | 240 个会话的特征包 |
| `models/lr_classifier.mlpackage` | Logistic Regression Core ML 模型（主部署） |
| `models/rf_classifier.mlpackage` | Random Forest Core ML 模型（回退） |
| `models/xgb_classifier_v2.mlpackage` | XGBoost Core ML 模型（中期候选） |
| `swift-app/` | macOS SwiftUI 应用源码 |

## 限制与边界

- **样本量**：12 个用户，240 个会话，Poor 类仅 7 个样本
- **弱标签**：基于规则生成的 recovery_label，不是真实用户反馈
- **不是医疗工具**：MVP 是工程演示，不提供医学诊断
- **仅 macOS**：SwiftUI 应用仅支持 macOS 14.0+

## 下一步（中期）

1. 引入更多公开睡眠数据（CMI Detect Sleep States）
2. 增加 HRV 和更多时序特征
3. 探索深度学习模型（1D CNN/LSTM）
4. 添加真实用户反馈标签以减少弱标签的偏差
