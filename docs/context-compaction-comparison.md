# 上下文压缩方案对比：QuantClaw、OpenClaw 与 nanobot 系项目

本文从**产品能力**与**实现侧重点**对比 QuantClaw 当前的上下文压缩设计，并参考 [OpenClaw 官方 Compaction 概念文档](https://github.com/openclaw/openclaw/blob/main/docs/concepts/compaction.md) 与 **nanobot**（[HKUDS/nanobot](https://github.com/HKUDS/nanobot)，自述受 OpenClaw 启发的轻量实现）。另有一个名称相近的 **Nanocoder**（Nano-Collective）项目带有独立的 [Context Compression 文档](https://github.com/Nano-Collective/nanocoder/blob/main/docs/features/context-compression.md)，其「按窗口百分比自动压缩、保守/激进模式」等与下面两类不完全相同，故单独一句说明，不展开等同为 nanobot。

**说明**：仓库内 [规格 spec](specs/context-multi-stage-compaction.md) 已以 **嵌入式/资源受限部署** 为首要约束重写；下表仍有助于与 OpenClaw / nanobot 对照，但 QuantClaw 的**默认策略**更偏向「少额外 LLM、多确定性截断」。

---

## 总览表

| 维度 | OpenClaw | QuantClaw（当前） | nanobot（HKUDS，概括） |
|------|----------|-------------------|-------------------------|
| **核心目标** | 长会话在窗口内可持续；压缩结果写入会话，供后续轮次使用 | 与 OpenClaw 工作区/协议兼容前提下，在 C++ 网关内控制上下文体积 | 极简 Agent：ContextBuilder + 会话历史 + 文件记忆，强调可读与轻量 |
| **自动压缩** | 默认开启；接近窗口上限或**溢出错误**时触发，可重试原请求 | 组装阶段有**确定性**条数/窗口防护；**LLM 摘要**默认关闭，仅在 `compaction.strategy` 为 summarize/multistage 且走溢出恢复时使用 | 更多依赖**历史长度/记忆文件**与简单策略，而非与 OpenClaw 同级的一整套「会话内 compaction 管线」文档 |
| **手动压缩** | `/compact` 与可带指令的摘要引导 | 若网关/CLI 暴露等价 RPC 或命令则与 OpenClaw 对齐度取决于实现；核心能力在引擎与溢出路径 | 以项目 CLI/频道命令为准，通常不是 OpenClaw 那套完整 `/compact` 语义 |
| **工具轮边界** | 显式保证 assistant 工具调用与 `toolResult` 成对切块 | `ChunkByMaxTokensAtomic`：assistant(tool_use) + 连续 user(tool_result) 不拆块 | 视具体版本；轻量项目往往不在文档中单列「分块摘要」细节 |
| **摘要模型** | 可配置 **`agents.defaults.compaction.model`**，与主模型分离 | 当前摘要走 **`resolve_provider()` 与主模型**；未实现单独的 `compaction.model` | 一般无独立「压缩专用模型」配置 |
| **可插拔压缩** | 插件可 **`registerCompactionProvider`**，配置 `provider` 走自定义管线 | 可插拔 **`ContextEngine`**；自定义引擎需自行接摘要或包装默认引擎 | 扩展点多在工具与 workspace，而非 OpenClaw 式 compaction provider |
| **持久化** | 摘要写入 **session 转录（JSONL）**；全量历史仍在磁盘 | 运行时压缩 + 可选会话侧 `SessionCompaction`；与 OpenClaw 持久化细节需按版本对齐 | 会话文件 + `MEMORY.md` / 日志等；**记忆整理（consolidation）**与「对话 compaction」是不同层 |
| **溢出前记忆** | 压缩前可触发 **memory flush**，提醒写入长期记忆 | 未内置同一套「压缩前静默一轮写盘」；可由用户/插件/hooks 补充 | 依赖用户与 workspace 约定 |
| **其它** | **OpenAI 服务端 compaction**、**identifierPolicy**、**session pruning** 等与「本地摘要 compaction」并列 | BM25/工具剪枝等与 OpenClaw 的 pruning 概念部分对应；服务端 compaction 未等同实现 | 窗口与历史：常见为截断 + 记忆文件，而非完整 multi-stage 文档化 |

---

## OpenClaw：默认即「会摘要、会落盘、可换模型」

OpenClaw 将 **compaction** 定义为接近上限时对**较早轮次做摘要**，并把摘要**写回会话历史**，下一轮模型看到的是「摘要 + 近期完整消息」。文档明确：

- 分块时保持 **tool 与 toolResult 配对**（与 QuantClaw 原子分段目标一致）。
- **自动压缩默认开启**：窗口将满或 **API 返回上下文溢出**时都会触发并可重试。
- 配置集中在 **`agents.defaults.compaction`**（mode、target tokens、`identifierPolicy`、可选 **`model` 覆盖** 等）。
- 与 **session pruning**（仅裁工具结果、不落盘摘要）区分清晰。

QuantClaw 在行为上**对齐的方向**是：工具轮不拆、溢出可重试、可多阶段 chunk-merge；**差异**主要在于：默认不启用 LLM 摘要（`strategy: truncate`）、摘要模型未单独配置、以及会话 JSONL 元数据与 OpenClaw 是否逐字段一致需随版本核对。

官方入口（便于对照）：[Compaction 概念说明](https://github.com/openclaw/openclaw/blob/main/docs/concepts/compaction.md)（站点镜像如 [openclaws.io/docs](https://openclaws.io/docs/concepts/compaction/)）。

---

## nanobot（HKUDS）：轻量管线，「压缩」分散在记忆与上下文

nanobot 强调 **极少代码量** 与 **OpenClaw 式工作区/技能** 的启发，但**公开文档并不像 OpenClaw 那样把「Compaction」作为独立一章**来描述 multi-stage、identifier policy、独立摘要模型等。

典型特点（概括自架构描述与社区文章）：

- **ContextBuilder**：拼装 system、bootstrap、memory、skills、history，偏「组上下文」而非一整套可配置的 compaction 子系统。
- **Memory**：长期 `MEMORY.md`、按日日志等；**旧日志合并/整理**承担部分「信息浓缩」，与 OpenClaw 里「对对话做摘要并写回 session」是不同机制。
- **上下文窗口**：常见做法是控制历史条数或简单截断策略，而非完整 OpenClaw 级 compaction 配置矩阵。

因此与 QuantClaw 对比时：**nanobot 更像「轻量 + 记忆文件驱动」**；**QuantClaw 更像「向 OpenClaw 对齐的网关 + 可选与 OpenClaw 同类的 LLM 多阶段压缩」**，默认偏保守（无额外摘要调用）。

---

## Nanocoder（易混淆）：另一套「Context Compression」产品

[Nanocoder 的 Context Compression 文档](https://github.com/Nano-Collective/nanocoder/blob/main/docs/features/context-compression.md) 描述的是 **autoCompact 阈值（如窗口用量 60%）**、**conservative/aggressive** 模式、`/compact` 与预览等——这是**另一款产品**的配置面，与 HKUDS nanobot **不是同一仓库**。对比 QuantClaw 时只需知道：三者都解决「上下文过长」，但 **OpenClaw / QuantClaw** 与 **Nanocoder** 的配置项与触发条件**不可一一对应**。

---

## 如何在 QuantClaw 里「贴近 OpenClaw」

若你希望行为更接近 OpenClaw 文档中的体验：

1. 在配置里将 **`compaction.strategy`** 设为 **`summarize` 或 `multistage`**，使溢出时走 LLM 摘要而非纯截断。  
2. 关注 **`minMessagesForMultistage`、`maxChunkTokens`、`maxSummaryCallsPerTurn`**，避免极端会话打满调用上限。  
3. 需要 **独立摘要模型** 时：当前需改代码或后续增加与 OpenClaw `compaction.model` 等价的配置（规格与技术方案中已作为开放项）。  
4. 需要 **压缩前写 MEMORY**：依赖工作流或插件/hooks，而非网关内置的 OpenClaw memory flush 一轮。

更细的 QuantClaw 行为说明见 [context-compaction.md](./context-compaction.md)；契约与实现见 [specs/context-multi-stage-compaction.md](./specs/context-multi-stage-compaction.md)。
