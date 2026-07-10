# Matt Pocock Skills 接入与 Spec 规划计划

## Summary

本计划只覆盖两个目标：

1. 将 `https://github.com/mattpocock/skills` 按官方方式接入当前项目
2. 基于该技能集建立后续的 spec 编写流程

当前已锁定的流程为：

1. 使用官方安装器：`npx skills@latest add mattpocock/skills`
2. 安装后运行：`/setup-matt-pocock-skills`
3. Spec 工作流采用：`/grill-with-docs -> /to-spec`
4. Issue tracker 默认先使用：本地 Markdown
5. 技能生成的文档默认保存到：`docs/agents`
6. 保留后续切换到 GitHub 仓库 `git@github.com:tangzzz-fan/sleep-recovery.git` 的路径

本计划的重点不是立即实现，而是确保安装、配置、spec 产出路径和未来迁移路径都是决策完整的。

## Current State Analysis

经过只读检查，当前工作区实际情况如下：

1. 当前项目目录只有一个已落地文档目录：`docs/`
2. 当前 `.trae/` 下只有 `documents/`，没有自定义 skill 目录
3. 当前仓库内已存在的文档包括：
   - [sleep-recovery-project-plan.md](file:///c:/Users/AppleXian/Desktop/Swift%20Playground/docs/sleep-recovery-project-plan.md)
   - [mvp-milestone-plan.md](file:///c:/Users/AppleXian/Desktop/Swift%20Playground/.trae/documents/mvp-milestone-plan.md)
4. 当前项目尚未存在：
   - `AGENTS.md`
   - `CLAUDE.md`
   - `docs/agents/`
   - `.trae/skills/`
5. 外部仓库 `mattpocock/skills` 的只读检查结果表明：
   - 官方安装方式是 `npx skills@latest add mattpocock/skills`
   - 安装后推荐先运行 `/setup-matt-pocock-skills`
   - 与 spec 编写最相关的技能是 `/to-spec`
   - 如果需求尚未充分澄清，推荐先运行 `/grill-with-docs`

因此，当前项目还没有任何可以直接调用的 Matt Pocock skill，必须先完成安装与 repo 级配置，才能进入基于 skill 的 spec 编写。

## Assumptions & Decisions

### 已锁定决策

1. 采用官方安装器，而不是手动拷贝 skill
2. spec 工作流采用 `grill-with-docs -> to-spec`
3. issue tracker 默认先使用本地 Markdown
4. 生成的 spec / ADR / agent 文档默认放到 `docs/agents`
5. 当前阶段先不依赖 GitHub Issues，避免配置阻塞
6. 未来如果仓库 remote 接入 `git@github.com:tangzzz-fan/sleep-recovery.git`，可以再次运行 `/setup-matt-pocock-skills` 切换到 GitHub Issues

### 重要假设

1. 当前环境后续能够运行 `node` / `npm` / `npx`
2. 当前 agent 环境支持安装并识别外部 skills
3. `mattpocock/skills` 仓库的安装器会把所选 skill 安装到当前 agent 可识别的位置
4. `docs/agents` 可以作为项目内长期保留的文档目录

### 风险决策

1. 先用本地 Markdown issue tracker，降低对远程仓库和 CLI 凭证的依赖
2. spec 先落本地文档，再决定是否同步到 GitHub Issues
3. 如果 `XGBoost / LightGBM` 相关 spec 后续太复杂，先用 `grill-with-docs` 把问题边界澄清，再调用 `/to-spec`

## Proposed Changes

### 目标文件与目录

后续执行阶段将涉及以下文件或目录：

1. `c:\Users\AppleXian\Desktop\Swift Playground\.trae\`
2. `c:\Users\AppleXian\Desktop\Swift Playground\docs\agents\`
3. `c:\Users\AppleXian\Desktop\Swift Playground\AGENTS.md` 或 `CLAUDE.md`
4. `c:\Users\AppleXian\Desktop\Swift Playground\.trae\skills\` 或安装器使用的 agent 技能目录

### 阶段 1：安装 Matt Pocock Skills

#### 要做什么

1. 在当前项目目录执行官方安装命令：
   - `npx skills@latest add mattpocock/skills`
2. 在安装选择界面中，至少勾选：
   - `/setup-matt-pocock-skills`
   - `/grill-with-docs`
   - `/to-spec`
3. 如安装器支持额外工程 skill，建议一并安装：
   - `/to-tickets`
   - `/implement`
   - `/tdd`

#### 为什么这样做

1. 这是上游 README 推荐的标准路径
2. 可以最大限度保持后续 skill 说明、目录布局和调用方式与上游一致
3. 后续如需升级或重新安装，维护成本最低

#### 成功标准

1. 当前 agent 能识别并调用 `/setup-matt-pocock-skills`
2. 当前 agent 能识别并调用 `/grill-with-docs`
3. 当前 agent 能识别并调用 `/to-spec`

### 阶段 2：运行 Repo 级初始化配置

#### 要做什么

安装完成后，立即运行 `/setup-matt-pocock-skills`，并按已锁定决策回答配置问题：

1. Issue tracker：
   - 先选择 `Local markdown`
2. Triage labels：
   - 若当前项目无既有标签体系，则先接受该 skill 的默认思路或最小标签集
3. Domain docs / generated docs 保存位置：
   - 选择 `docs/agents`

#### 预期输出

1. `AGENTS.md` 或 `CLAUDE.md` 中增加 `Agent skills` 配置块
2. `docs/agents/` 被创建
3. 与本地 issue tracker / 文档布局相关的说明被落地

#### 为什么这样做

1. `grill-with-docs` 和 `to-spec` 依赖 repo 级上下文
2. 不先做 repo setup，spec skill 可能不知道 issue tracker 和文档位置
3. 本地 Markdown 比 GitHub Issues 更适合当前这个尚未完全远程化的项目阶段

#### 成功标准

1. 生成的 repo 配置能被后续工程类 skill 读取
2. `docs/agents` 成为后续 spec / ADR 的默认落点

### 阶段 3：为当前项目建立 Spec 输入上下文

#### 要做什么

在正式调用 spec skill 之前，把当前已有项目规划作为输入上下文统一起来，至少包括：

1. [sleep-recovery-project-plan.md](file:///c:/Users/AppleXian/Desktop/Swift%20Playground/docs/sleep-recovery-project-plan.md)
2. [mvp-milestone-plan.md](file:///c:/Users/AppleXian/Desktop/Swift%20Playground/.trae/documents/mvp-milestone-plan.md)
3. 当前已确定的关键决策：
   - 场景：睡眠恢复评估
   - 数据源：Kaggle 公开睡眠数据为 MVP 主训练数据
   - 输入方式：Swift 端接收预计算特征包
   - 模型方向：`XGBoost / LightGBM` 为主部署路线
   - 项目目标：体现真实数据适配、模型取舍和 Core ML 优化分析能力

#### 为什么这样做

1. `/to-spec` 是“综合当前对话”的 skill，不是自动补全所有缺失前提的魔法工具
2. 先把关键上下文锁清楚，生成的 spec 会更可执行
3. 这样也便于后续从本地 Markdown issue tracker 迁移到 GitHub Issues

#### 成功标准

1. 需要写入 spec 的范围、边界和关键约束已经清楚
2. 不会在 skill 运行时才临时决定高影响架构问题

### 阶段 4：运行 Spec 工作流

#### 要做什么

按以下顺序调用技能：

1. 先运行 `/grill-with-docs`
   - 目标是把当前 MVP 里的高风险问题问透
   - 特别关注：
     - Kaggle 数据集最终选型
     - 弱标签如何定义
     - `XGBoost / LightGBM` 到 Core ML 的转换边界
     - Swift 端需要展示哪些图和解释信息
2. 完成澄清后运行 `/to-spec`
   - 让 skill 基于已讨论内容生成正式 spec
   - spec 默认输出到 `docs/agents`

#### 为什么这样做

1. 你已经明确要求 spec 需要更详细
2. 当前项目的难点不在“写一份文档”，而在于把数据、训练、Core ML、Swift 端职责边界说明清楚
3. `grill-with-docs -> to-spec` 是最符合你当前阶段的工作流

#### 预期输出

1. 一份更详细的 spec 文档
2. 必要时附带：
   - 术语说明
   - 模块边界
   - 风险记录
   - ADR 或 agent docs

#### 成功标准

1. spec 能覆盖 MVP 的数据流、模块职责、交付边界和验收标准
2. spec 生成后可以直接进入任务拆分或实现阶段

### 阶段 5：为未来 GitHub 仓库保留迁移路径

#### 要做什么

待项目 remote 切换到 `git@github.com:tangzzz-fan/sleep-recovery.git` 后，再次运行 `/setup-matt-pocock-skills` 或补充配置，将 issue tracker 从本地 Markdown 切换到 GitHub Issues。

#### 为什么这样做

1. 先用本地 Markdown 可以避免当前阶段被远程配置阻塞
2. 等 GitHub 仓库准备好，再把 spec / tickets / issues 流程迁移过去更稳妥

#### 成功标准

1. 本地 spec 流程和未来 GitHub issue 流程可以平滑衔接
2. 不需要推翻 `docs/agents` 文档结构

## Verification Steps

后续执行时应按以下顺序验证：

1. 验证官方安装器能成功运行
2. 验证 `/setup-matt-pocock-skills`、`/grill-with-docs`、`/to-spec` 已可被当前 agent 识别
3. 验证 `/setup-matt-pocock-skills` 产出的 repo 配置确实指向 `docs/agents`
4. 验证使用本地 Markdown 作为 issue tracker 时，spec 相关 skill 可以正常工作
5. 验证 `grill-with-docs -> to-spec` 能基于现有项目文档产出更细化的 spec
6. 验证未来切换到 GitHub Issues 时，不需要推翻当前 spec 文档布局
