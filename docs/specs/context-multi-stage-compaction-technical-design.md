# 技术方案：上下文压缩（嵌入式部署导向）

| 属性 | 值 |
|------|-----|
| 状态 | Draft |
| 关联规格 | [context-multi-stage-compaction.md](./context-multi-stage-compaction.md) |
| 目标读者 | 实现者、部署架构师 |

---

## 1. 文档目的

说明在 **QuantClaw 可安装于嵌入式/边缘设备** 这一目标下，上下文压缩相关模块的职责划分、与「推理侧优化」的边界、推荐配置画像及实现现状。本文**不**重复抄录 OpenClaw 或轻量框架的营销表述，而是以 **资源边界 + 网关职责** 为出发点。

---

## 2. 独立判断：三类常见误解

### 2.1 「多阶段 LLM 摘要 = 先进默认」

**判断**：在**网关进程 + 远程 API** 架构中，多阶段摘要意味着 **多次额外的 completion**。对嵌入式场景，瓶颈常常是 **链路延迟、计费、熔断风险**，而不是「摘要写得多优雅」。因此实现上已将 **truncate 设为默认**，L3（`summarize`/`multistage`）为显式 opt-in。这与部分云端 Agent「默认接近窗口就摘要」的产品取向**刻意不同**。

### 2.2 「上下文压缩 = 模型 KV 优化」

**判断**：边缘推理文献中大量讨论 **KV cache 随序列长度线性增长**、量化与 eviction（例如对资源受限环境下长 prompt 的讨论见 [KeyDiff 类工作](https://arxiv.org/html/2504.15364v1) 及产业界对动态 KV 量化的综述）。QuantClaw **当前**作为应用网关，**不**实现 KV 层算法；若设备上另跑 llama.cpp/vLLM，应由推理栈配置 context 与 cache 策略。网关侧做的是 **减少送入模型的 token 数**——与推理优化 **互补**，不是替代。

### 2.3 「轻量 = 少写代码」

**判断**：轻量部署的核心是 **默认路径零额外网络与可预期行为**。代码上反而要保证 **L0 剪枝 + L2 截断** 稳定、可测；L3 是附加模块。「少调用」优于「少行数」。

---

## 3. 架构分层与代码映射

| 规格层 | 主要模块 | 说明 |
|--------|----------|------|
| **L0** | `DefaultContextEngine::Assemble`、`ContextPruner`、工具结果截断常量 | 纯本地，无摘要 LLM |
| **L1** | 同上，组装结果 `estimated_tokens` 与窗口比较 | 无摘要 LLM |
| **L2** | `DefaultContextEngine::CompactOverflow`（`strategy=truncate`） | 溢出后截断重试，`AgentLoop` 控制重试次数 |
| **L3** | `MultiStageCompaction`、`ChunkByMaxTokensAtomic`、`AgentLoop::SummarizeForCompaction` | 仅当 `compaction.strategy` 为 summarize/multistage |

**数据流（溢出）**：`AgentLoop` → `CompactOverflow` → 若 L3 则 `CompactMultiStage` → 摘要请求仍走 `resolve_provider()->ChatCompletion`（无工具）。

---

## 4. 嵌入式部署的额外约束（工程化）

### 4.1 内存

- 会话 JSONL 与内存中 `vector<Message>` 随对话增长；L0 条数上限是 **第一道闸**。
- L3 合并阶段应避免在进程内保存多份完整历史副本（当前实现以流式块处理为主，仍须在极端 chunk 数下警惕峰值内存——未来可由 `merge_tree_threshold` 与串行摘要约束）。

### 4.2 网络与 API

- 每次 `SummarizeForCompaction` 是一次独立 RTT；弱网下应 **强烈倾向 truncate**。
- `max_summary_calls_per_turn` 为硬顶；嵌入式建议 **≤4**，与规格一致。

### 4.3 磁盘

- 全量历史仍在 JSONL；压缩若写入摘要条目，应遵循会话模块的写入策略，避免与「嵌入式闪存寿命」冲突——**高频小写优于低频大写** 的一般原则适用于产品层策略（非本文件实现细节）。

### 4.4 可观测性

- 日志中区分 `truncate` / `summarize` / `multistage` 路径，便于现场日志只有几 MB 时仍可读。

---

## 5. `system.deploymentProfile: embedded`

在顶层 `system` 中设置 `"deploymentProfile": "embedded"` 时，加载配置后会对 **`agent` 中未在 JSON 里出现的键** 应用保守默认值（与 `kEmbeddedProfile*` 常量一致，见 `constants.hpp`）：

| 键（camelCase 或 snake_case） | 嵌入式缺省 |
|-------------------------------|------------|
| `contextWindow` | 8192 |
| `compactMaxMessages` | 40 |
| `compactKeepRecent` | 12 |
| `compactMaxTokens` | 24000 |
| 整个 `compaction` 对象缺失时 | `strategy: truncate`，`maxSummaryCallsPerTurn: 4`，`minMessagesForMultistage: 12`，`maxChunkTokens: 8192 |

若用户在 `agent`、`agents.defaults` 或 `llm` 中**显式**写了上述任一键，则 **不覆盖**该键。`config.example.json` 中 `system.deploymentProfile` 可为空字符串（等价于未启用 profile）。

**等价手写示例**（与 profile 自动合并效果一致，供对照）：

```json
{
  "system": { "deploymentProfile": "embedded" },
  "llm": { "model": "openai/your-model" }
}
```

若仍启用 L3，应在 JSON 中显式写出 `compaction` 并设置较小 `maxChunkTokens`、`maxOutputTokens`。

---

## 6. 实现现状与差距（截至文档编写时）

| 项 | 状态 |
|----|------|
| L0/L1/L2 | 已实现 |
| `CompactionRuntimeConfig` + `FromJson` | 已实现 |
| `ChunkByMaxTokensAtomic` + merge token 估算 | 已实现 |
| `AgentLoop` 默认引擎 + `SummarizeForCompaction` | 已实现 |
| `merge_tree_threshold` / 并行 chunk | 配置预留，未全面接线 |
| `system.deploymentProfile: embedded` 缺省键合并 | **已实现**（仅当对应键未出现在 `agent` / `agents.defaults` / `llm`） |
| 摘要专用模型 `compaction.model` | **未实现**，利于嵌入式用小模型摘要时需补 |

---

## 7. 分阶段交付（调整后的优先级）

| 阶段 | 内容 | 与嵌入式关系 |
|------|------|----------------|
| **维持** | L0/L2 默认稳定、测试覆盖 | 最高优先级 |
| **可选** | L3 在显式配置下可用，且强降级 | 现场按需开启 |
| **后续** | 树状 merge、并行 chunk | 默认不推荐嵌入式开启 |
| **后续** | `compaction.model` 与主模型分离 | 利于「主任务大模型 + 摘要小模型」降本 |

---

## 8. 风险

| 风险 | 缓解 |
|------|------|
| 摘要请求再次溢出 | 限制摘要输入块大小与 `max_output_tokens`；失败回 L2 |
| 嵌入式仍配 128K 窗口导致估算偏离 | 文档与 `doctor` 提示按实机模型填写 |
| 自定义 `ContextEngine` 未接摘要 | 文档说明；现场用默认引擎或自实现 L2 |

---

## 9. 参考文献与延伸阅读（不构成实现依赖）

- 资源受限环境下的长上下文与 KV 管理：边缘推理与 cache eviction 类研究（例：[arXiv:2504.15364](https://arxiv.org/html/2504.15364v1)）用于理解**推理层**瓶颈，与本网关应用层策略互补。
- 端侧 Agent 上下文与内存权衡的讨论（例：产业博客对 **KV 与 RAM** 的量化分析）有助于向用户解释「为何默认 truncate」。

---

## 10. 修订历史

| 版本 | 日期 | 说明 |
|------|------|------|
| 0.1 | 2026-04-08 | 初稿：模块接线、原子分块 |
| **0.2** | **2026-04-08** | **重写：嵌入式约束、L0–L3 分层、误解澄清、配置画像、文献边界** |
| 0.3 | 2026-04-08 | 实现 `system.deploymentProfile: embedded` |
