# 规格说明：上下文压缩（嵌入式部署导向）

| 属性 | 值 |
|------|-----|
| 状态 | Draft |
| 首要部署假设 | **资源受限环境**（嵌入式 SoC、工控机、边缘网关）：有限 RAM/闪存、可能弱网或按量计费 API、需可预期行为 |
| 适用范围 | QuantClaw 网关进程内的上下文组装、溢出恢复、会话持久化；与模型推理侧 KV 优化正交 |
| 相关实现 | `ContextPruner`、`DefaultContextEngine`、`MultiStageCompaction`、`AgentLoop`、`SessionCompaction` |

**相关文档**：[技术方案（嵌入式导向）](./context-multi-stage-compaction-technical-design.md) · [用户向说明：上下文压缩](../context-compaction.md)

---

## 1. 问题重述：我们到底在压缩什么

QuantClaw 是 **C++ 网关**：在典型部署里，它维护会话状态、组装发往**远端或本地** LLM 的请求。所谓「上下文压缩」发生在 **应用层消息列表**（JSON / `Message` 向量）层面，目的是：

1. 使单次请求不超过供应商或本地推理引擎的 **上下文窗口**；
2. 控制 **网关进程** 持有的历史体积（内存、磁盘上的 JSONL）；
3. 在弱网/计费场景下，**尽量少引入额外的 LLM 往返**。

这与 **模型内部的 KV cache 管理、量化、稀疏注意力**（常见于端侧推理文献，如 KV 缓存随序列线性增长、边缘设备内存墙）是**不同层次**的问题：后者属于推理运行时；本规格**不**规定推理引擎行为，但若目标设备同时跑小模型，**应用层更激进地缩短 prompt** 与「小上下文窗口」现实是一致的。

**独立判断**：「多阶段 chunk + LLM merge」在数据中心长会话产品里很合理；在**嵌入式 + 计费/弱网**场景里，它应是**少数派路径**——默认应用 **无 LLM 的确定性压缩**，仅在显式策略或溢出且允许时才调用摘要。

---

## 2. 嵌入式场景下的约束（非功能需求）

下列约束优先于「摘要质量最大化」：

| 维度 | 要求 |
|------|------|
| **RAM** | 会话与组装路径上的数据结构有上界；避免在网关内无界缓存「待合并」的巨型字符串。 |
| **网络 / API** | 默认不增加额外 completion；若启用摘要，单次用户 turn 内摘要调用次数必须有硬上限。 |
| **闪存 / 磁盘** | 会话 JSONL 增长与压缩写入策略应与既有日志轮转策略一致；避免高频大块重写（见现有 `system.log*` 策略）。 |
| **确定性** | 默认路径（截断、条数限制、工具剪枝）行为可离线复现，便于现场排障。 |
| **功耗与实时** | 避免默认开启「接近窗口即自动 LLM 摘要」导致空闲时仍周期性唤醒上游模型（若未来实现类似能力，须单独开关）。 |

---

## 3. 分层策略（规格核心）

压缩能力按 **成本与语义损失** 分为四层；实现上**必须**自低层向上尝试，且高层可选。

| 层级 | 名称 | 是否调用 LLM | 典型手段 |
|------|------|----------------|----------|
| **L0** | 结构化预算 | 否 | `auto_compact` 按条数丢弃远端历史；`ContextPruner` 剪工具结果；`compact_max_tokens` 等 |
| **L1** | 窗口守卫 | 否 | 组装阶段对估算 token + `max_tokens` 相对 `context_window` 的守卫 |
| **L2** | 溢出恢复（确定性） | 否 | `CompactOverflow` 在 `strategy=truncate`（默认）：保留 system + 最近 N 条 + 提示语 |
| **L3** | 语义压缩（可选） | 是 | `strategy=summarize|multistage`：`MultiStageCompaction`、chunk 原子性、merge |

**规格立场**：

- **默认启用 L0–L2**，**默认不启用 L3**。
- L3 仅在配置显式允许 **且** 运行时注入摘要能力时生效；失败必须 **无损降级到 L2**。
- 「多阶段」是为 **单次摘要无法装入模型输入上限** 而设，不是嵌入式场景的默认形态；嵌入式配置应倾向 **更小的 `max_chunk_tokens`、更低的 `max_summary_calls_per_turn`**，甚至永远关闭 L3。

---

## 4. 术语

| 术语 | 含义 |
|------|------|
| **Compaction** | 广义：缩短较早历史以利后续请求；狭义可指 L3 的 LLM 摘要（本仓库配置里 `compaction.strategy`）。 |
| **Chunk / Merge** | L3 中将历史分段摘要再合并；分块须遵守 **atomic turn**（含 `tool_use` 的 assistant 与后续 `tool_result` user 不拆开）。 |
| **Overflow recovery** | 供应商返回上下文溢出错误后的重试路径，调用 `CompactOverflow`。 |
| **嵌入式配置画像** | 一组推荐默认值：小 `context_window`、激进 L0、`truncate`、极低或零 L3 调用配额（实现为配置约定，见技术方案）。 |

---

## 5. 功能性需求

### FR-1：默认路径

- 未显式启用 L3 时，溢出与预压缩**不得**依赖 LLM 摘要。
- `compaction.strategy` 默认等价于 **truncate**（与实现一致）。

### FR-2：L3 启用条件

- 配置为 `summarize` 或 `multistage`；
- 摘要函数可用；
- 非 system 消息条数 ≥ `min_messages_for_multistage`（可配置）；
- 当前 turn 摘要调用次数未超过 `max_summary_calls_per_turn`。

### FR-3：分块与合并（L3）

- 分块满足 **atomic turn**；单段超长时整段入块，由 L0 工具截断策略处理细节。
- Merge 体积估算须与全链路 token 估算一致（如 `ContextPruner::EstimateTokens`），避免误判。

### FR-4：溢出重试

- 与 `kOverflowCompactionMaxRetries` 协作；每次重试前压缩后估算规模应下降，否则记录告警并仍允许有限次重试或降级。

### FR-5：与「外置记忆」的关系（推荐）

- 嵌入式场景宜鼓励 **把稳定事实写入工作区文件**（如 `MEMORY.md`），减少依赖超长对话内状态。压缩**不能**替代显式持久化策略；规格上鼓励与文档化 onboarding 一致。

---

## 6. 非目标

- 不规定推理引擎 KV eviction / 量化（若未来集成本地 llama.cpp 等，另立规格）。
- 不保证摘要语义与人工审阅一致。
- 向量检索、混合记忆等可与本规格并行演进。

---

## 7. 配置（逻辑名）

与实现中 `CompactionRuntimeConfig` 对齐；嵌入式部署建议见技术方案「配置画像」。

核心键：

- `compaction.strategy`：`truncate` | `summarize` | `multistage`
- `compaction.max_summary_calls_per_turn`：嵌入式宜 **低**（如 ≤4）或关闭 L3
- `compaction.max_chunk_tokens`、`compaction.max_output_tokens`：与实际上游窗口匹配，避免摘要请求本身溢出

`auto_compact`、`compact_*` 仍只管 L0，与 L3 正交。

持久化相关键：
- `agent.compactPersist`：布尔，默认 `true`；L0 压缩触发时是否将结果写入磁盘
- `agent.maxArchivedTranscripts`：整数，默认 `5`（嵌入式 `2`）；保留的归档数
- `agent.preCompactMemoryExtract`：布尔，默认 `false`；是否在压缩前提取持久事实到 `memory/`

---

## 8. 错误与降级

| 场景 | 行为 |
|------|------|
| L3 失败、限流、空摘要 | 降级 L2 截断 |
| 达到摘要次数上限 | 降级 L2，并记录 warn |

---

## 9. 测试与验收

- L0/L2：无 LLM mock 下可单测。
- L3：mock provider；嵌入式回归集应包含 **默认无额外 completion** 的断言。
- 原子分块：含 tool 对的合成历史不被拆散。

---

## 10. 与 OpenClaw 生态的关系

OpenClaw 默认更强调「接近窗口即压缩 + 会话落盘摘要」的完整体验。QuantClaw 在**嵌入式优先**目标下，**刻意**将同等能力拆成显式层级，避免在弱网设备上默认放大 API 调用；对齐的是**文件格式与协议**，不一定是默认行为一致。

---

## 11. 压缩持久化

### FR-6：压缩结果持久化

L0 自动压缩触发时，**默认**将压缩结果持久化到磁盘（`compactPersist: true`），而非每次请求都从原始 JSONL 重新加载并重新压缩。

**流程**：

1. `DefaultContextEngine::Assemble` 触发 L0 压缩时，通过回调通知 `SessionManager::CompactSession`
2. 旧 JSONL 被归档为 `{session_id}.jsonl.compacted.{timestamp}`
3. 新 JSONL 写入压缩标记行 + 保留的近期消息
4. 后续 `GetHistory` 直接加载压缩后的 JSONL（无需重新压缩）

**归档策略**：

- `maxArchivedTranscripts`：保留的归档数量上限（默认 5，嵌入式 2）
- 超出上限时自动删除最旧的归档
- 归档文件遵循命名规则 `{session_id}.jsonl.compacted.{ISO8601}`

### FR-7：压缩前记忆提取（可选）

启用 `preCompactMemoryExtract: true` 时，压缩前从即将丢弃的消息中确定性提取持久事实：

- 工具调用结果（成功/失败）
- 用户修正（"不对"、"I meant"）
- 显式 "记住" 指令

提取结果追加到工作区 `memory/` 日志。**不调用 LLM**，纯确定性扫描。

### 配置

```json
{
  "agent": {
    "compactPersist": true,
    "maxArchivedTranscripts": 5,
    "preCompactMemoryExtract": false
  }
}
```

---

## 12. 开放问题

- 本地推理（Ollama/llama.cpp）时，`context_window` 与供应商声明如何统一？

**已落地**：
- `system.deploymentProfile` 为 `"embedded"` 时，对未在 JSON 中显式给出的 agent 键应用保守默认（见技术方案 §5）。
- 压缩结果持久化（`compactPersist`）默认开启，`sessions.compact` RPC 也会持久化结果。

---

## 13. 修订历史

| 版本 | 日期 | 说明 |
|------|------|------|
| 0.1 | 2026-04-08 | 初稿：chunk+merge 契约 |
| **0.2** | **2026-04-08** | **重写：嵌入式部署为首要约束，分层策略（L0–L3），默认确定性路径** |
| 0.3 | 2026-04-08 | `system.deploymentProfile: embedded` 缺省合并 |
| **0.4** | **2026-04-08** | **压缩持久化：`CompactSession`、归档策略、压缩前记忆提取** |
