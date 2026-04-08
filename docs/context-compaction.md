# QuantClaw 上下文压缩（Context Compaction）

本文说明 QuantClaw 如何在对话变长时控制上下文体积：包含**组装阶段的轻量压缩**、**模型报错「上下文溢出」时的恢复**，以及可选的 **LLM 多阶段摘要（chunk + merge）**。更细的契约与实现细节见 [规格说明](specs/context-multi-stage-compaction.md) 与 [技术方案](specs/context-multi-stage-compaction-technical-design.md)。

与 **OpenClaw**、**nanobot** 等方案的差异见 [context-compaction-comparison.md](./context-compaction-comparison.md)。

**部署取向**：若目标是把网关跑在 **嵌入式 / 边缘设备**（有限内存、弱网或按量计费 API），规格上把 **确定性截断与剪枝（默认）** 放在 **LLM 摘要（可选）** 之前；详见规格中的 **L0–L3 分层** 与 [技术方案](./specs/context-multi-stage-compaction-technical-design.md)。可在 `system` 中设置 **`"deploymentProfile": "embedded"`**，对未在 JSON 里写明的 `contextWindow`、`compact*`、`compaction` 等一键套用保守默认（显式键不被覆盖）。

---

## 为什么需要上下文压缩

Agent 每轮会把「系统提示 + 历史消息 + 当前用户输入」发给模型。历史里还可能包含多轮工具调用与长工具结果。若不控制：

- 请求会逼近或超过模型的**上下文窗口**，供应商返回错误（常见为 HTTP 400 + context length 相关说明）；
- 即使未报错，过长历史也会拖慢响应并增加费用。

QuantClaw 通过多层策略在**尽量保留近期可执行上下文**的前提下缩短较早内容。

---

## 整体结构：三层逻辑

| 层次 | 触发时机 | 是否调用 LLM | 作用 |
|------|----------|--------------|------|
| **上下文组装（Assemble）** | 每次发起对话请求前 | 否（默认） | 条数/窗口防护、工具结果剪枝等 |
| **溢出恢复（CompactOverflow）** | 供应商返回「上下文溢出」类错误后，重试前 | 可选 | 截断或摘要，缩小 `request.messages` |
| **会话持久化压缩** | 会话条目标记为需压缩时（如 RPC / 独立逻辑） | 可选 | 与运行时策略宜保持一致，见 `SessionCompaction` |

下面重点说明前两层（网关与 `AgentLoop` 主路径）。

---

## 1. 组装阶段：`DefaultContextEngine::Assemble`

在真正调用聊天接口之前，默认引擎会：

1. **自动压缩（auto_compact）**  
   当历史**条数**超过 `compact_max_messages` 时，丢弃更早消息，只保留最近 `compact_keep_recent` 条，并插入一条简短的 system 提示说明「已压缩」。

2. **工具结果剪枝**  
   通过 `ContextPruner` 等对过长工具输出做裁剪，减少 token。

3. **上下文窗口守卫**  
   若估算 token + `max_tokens` 超过窗口预留，会再做一次基于条数的收紧。

相关配置在 `AgentConfig` 中（JSON 里常出现在 `llm` 或 `agents.defaults` 下）：

| 配置项 | 含义 |
|--------|------|
| `autoCompact` / `auto_compact` | 是否启用上述按条数预压缩 |
| `compactMaxMessages` / `compact_max_messages` | 超过多少条开始压缩（默认 100） |
| `compactKeepRecent` / `compact_keep_recent` | 保留最近多少条（默认 20） |
| `compactMaxTokens` / `compact_max_tokens` | 与 token 相关的阈值（默认 100000） |

这一阶段**不**使用 `compaction.strategy` 里的 LLM 摘要；默认也不会为「组装」单独发起摘要请求（`trigger: assembleAndOverflow` 为预留能力）。

---

## 2. 溢出恢复：`CompactOverflow` 与重试

当底层 `LLMProvider` 抛出**上下文溢出**错误时，`AgentLoop` 会调用当前 `ContextEngine` 的 `CompactOverflow`，然后**有限次重试**同一轮请求（次数由常量 `kOverflowCompactionMaxRetries` 控制，当前为 3）。

压缩策略由 **`compaction.strategy`** 决定：

### `truncate`（默认）

- **不**调用额外 LLM。  
- 保留主 system 提示与若干**最近消息**，中间用一条 system 说明「溢出恢复、已删除较早内容」。  
- 成本低、行为确定，适合作为默认。

### `summarize` / `multistage`

- 在满足条数下限等条件时，通过**一次或多次**「仅摘要、无工具」的补全调用，把较早历史压成一段 system 承载的摘要，再保留最近若干条原始消息。  
- 代码路径相同：`multistage` 名称强调「大块历史会拆成多段再合并」；是否走多段由内部根据 token/条数自动决定。  
- 摘要走当前解析到的模型与 `resolve_provider()`，受 `max_summary_calls_per_turn` 等限制，避免极端历史打爆调用次数。

若摘要失败（网络、限流、异常等），会**回退到与 `truncate` 相同的截断行为**。

---

## 3. LLM 多阶段摘要（chunk + merge）在做什么

当历史足够大、单次摘要输入会过大时，实现会：

1. **按原子轮次分块**：例如含 `tool_use` 的 assistant 消息与紧随其后的 `tool_result` user 消息会保持在同一块内，避免拆碎工具轮。  
2. **对每块调用摘要**。  
3. **合并**各块摘要；若合并后仍超过目标 token 预算，可再跑一轮摘要（merge pass）。  
4. 合并体积用与主路径一致的 **token 估算**（`ContextPruner::EstimateTokens`），而不是简单按字符除 4。

块大小、目标 token、安全裕度等由 `compaction` 下各项配置控制（见下表）。

---

## 4. 配置：`compaction` 对象

在 `~/.quantclaw/quantclaw.json` 中，可把 `compaction` 放在 **`llm`** 下，或放在 **`agent` / `agents.defaults`** 下。若同时存在 `llm` 与 `agent`，且 **`agent` 里没有写 `compaction`**，则会沿用 `llm.compaction`，避免被空对象覆盖。

示例（与仓库 `config.example.json` 风格一致）：

```json
{
  "llm": {
    "model": "openai/qwen-max",
    "compaction": {
      "strategy": "truncate",
      "trigger": "overflowOnly",
      "maxChunkTokens": 16384,
      "minMessagesForMultistage": 8,
      "maxSummaryCallsPerTurn": 16,
      "maxOutputTokens": 4096,
      "summaryTemperature": 0.3
    }
  }
}
```

启用 LLM 摘要溢出恢复时，将 `strategy` 设为 `summarize` 或 `multistage` 即可。

| 字段 | 含义 |
|------|------|
| `strategy` | `truncate`（默认）\| `summarize` \| `multistage` |
| `trigger` | 当前实现仅 `overflowOnly`；`assembleAndOverflow` 为预留 |
| `maxChunkTokens` | 每块最大估算 token，用于分块 |
| `targetTokens` | 合并后目标规模；`0` 表示用 `context_window / 4` |
| `safetyMargin` | 超过 `targetTokens × safetyMargin` 时可能触发最终再摘要 |
| `minMessagesForMultistage` | 非 system 消息至少多少条才走 LLM 摘要路径 |
| `maxSummaryCallsPerTurn` | 单轮用户请求内摘要调用次数上限 |
| `maxOutputTokens` | 单次摘要补全的最大输出 token |
| `summaryTemperature` | 摘要请求的温度 |

`mergeTreeThreshold`、`parallelChunkSummaries` 等字段已在配置结构中预留，用于未来树状归并与并行分块，当前版本可能尚未全部接线，调参前可看源码或变更说明。

修改配置后可执行 `quantclaw config reload`（若网关支持热加载）或重启网关。

---

## 5. 自定义 `ContextEngine`

若通过 API 注入了**自定义** `ContextEngine`，网关将使用该实现，**不会**自动把内置的 `SummarizeForCompaction` 注入到你的引擎。若需要与默认行为一致的 LLM 摘要，需在自定义引擎内自行实现或包装默认引擎。

---

## 6. 与 OpenClaw 的关系

QuantClaw 的配置与消息形态与 OpenClaw 对齐方向一致；上下文压缩在能力上覆盖「截断 + 可选多阶段摘要」。会话 JSONL 中若写入压缩元数据（`compacted` 等），可与 UI 或调试工具配合使用，具体字段以会话模块为准。

---

## 7. 压缩持久化

默认启用 `compactPersist: true`，压缩结果会**持久化到磁盘**而非每次请求重新计算：

- 旧的完整 JSONL 被归档为 `{session_id}.jsonl.compacted.{timestamp}`
- 新 JSONL 只包含压缩标记和保留的最近消息
- 后续请求直接使用压缩后的历史，避免重复工作

相关配置：

| 键 | 默认值 | 说明 |
|-----|--------|------|
| `agent.compactPersist` | `true` | 是否将压缩结果写入磁盘 |
| `agent.maxArchivedTranscripts` | `5`（嵌入式 `2`） | 每个会话保留的归档数量 |
| `agent.preCompactMemoryExtract` | `false` | 压缩前是否从丢弃消息中提取持久事实 |

启用 `preCompactMemoryExtract` 后，即将丢弃的消息会被扫描以提取工具执行结果、用户纠正和 "记住" 指令，追加到工作区的 `memory/` 日志中。

---

## 8. 小结

- **默认**：只在组装阶段做确定性压缩 + 溢出时**截断**重试，无额外摘要费用。压缩结果**持久化**到磁盘。
- **可选**：将 `compaction.strategy` 设为 `summarize` / `multistage`，在溢出时用 **chunk + merge** 思路保留更多语义，失败则退回截断。  
- 详细需求与边界条件以 [规格说明](specs/context-multi-stage-compaction.md) 为准。
