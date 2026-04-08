// Copyright 2025 QuantClaw Contributors
// SPDX-License-Identifier: Apache-2.0

#include "quantclaw/core/default_context_engine.hpp"

#include "quantclaw/constants.hpp"
#include "quantclaw/core/context_pruner.hpp"

namespace quantclaw {

DefaultContextEngine::DefaultContextEngine(
    const AgentConfig& config, std::shared_ptr<spdlog::logger> logger)
    : config_(config), logger_(logger), compactor_(logger) {}

AssembleResult DefaultContextEngine::Assemble(
    const std::vector<Message>& history, const std::string& system_prompt,
    const std::string& user_message, int context_window, int max_tokens) {
  AssembleResult result;

  // --- Step 1: Auto-compaction (message count or token budget) ---
  std::vector<Message> effective_history = history;
  if (config_.auto_compact) {
    bool over_message_limit = static_cast<int>(effective_history.size()) >
                              config_.compact_max_messages;
    bool over_token_limit =
        config_.compact_max_tokens > 0 &&
        ContextPruner::EstimateTokens(effective_history) >
            config_.compact_max_tokens;

    if (over_message_limit || over_token_limit) {
      int keep = config_.compact_keep_recent;
      int total = static_cast<int>(effective_history.size());
      if (total > keep) {
        int removed = total - keep;
        std::vector<Message> kept_msgs(history.end() - keep, history.end());

        // Persist compaction to disk before modifying in-memory state
        if (config_.compact_persist && compact_persist_cb_ &&
            !session_key_.empty()) {
          std::vector<SessionMessage> session_msgs;
          session_msgs.reserve(static_cast<size_t>(keep));
          for (const auto& m : kept_msgs) {
            SessionMessage sm;
            sm.role = m.role;
            sm.content = m.content;
            session_msgs.push_back(std::move(sm));
          }
          SessionManager::CompactionMetadata meta;
          meta.original_count = total;
          meta.kept_count = keep;
          meta.strategy = "truncate";
          compact_persist_cb_(session_key_, session_msgs, meta);
        }

        effective_history = std::move(kept_msgs);
        effective_history.insert(
            effective_history.begin(),
            Message{
                "system",
                "[Context compaction: " + std::to_string(removed) +
                    " earlier messages were removed. "
                    "Your system instructions remain active. "
                    "Refer to the system prompt for your identity "
                    "and capabilities.]"});
        logger_->info("Auto-compacted history: {} -> {} messages (trigger: {})",
                      total,
                      static_cast<int>(effective_history.size()),
                      over_message_limit ? "message_count" : "token_budget");
      }
    }
  }

  // --- Step 2: Tool result pruning ---
  ContextPruner::Options prune_opts;
  effective_history = ContextPruner::Prune(effective_history, prune_opts);

  // --- Step 3: Assemble context ---
  std::vector<Message> context;
  if (!system_prompt.empty()) {
    context.push_back(Message{"system", system_prompt});
  }
  for (const auto& msg : effective_history) {
    context.push_back(msg);
  }
  context.push_back(Message{"user", user_message});

  // --- Step 4: Context window guard ---
  int estimated = ContextPruner::EstimateTokens(context);
  if (estimated + max_tokens > context_window - kContextWindowMinTokens) {
    logger_->warn(
        "Context window guard: estimated {} tokens + {} max_tokens exceeds "
        "window {} (min reserve {}). Forcing compaction.",
        estimated, max_tokens, context_window, kContextWindowMinTokens);

    int keep =
        std::min(config_.compact_keep_recent, static_cast<int>(context.size()));
    if (keep < static_cast<int>(context.size())) {
      std::vector<Message> compacted;
      if (!system_prompt.empty()) {
        compacted.push_back(Message{"system", system_prompt});
      }
      compacted.push_back(
          Message{"system",
                  "[Context compaction forced: context window nearly full. "
                  "Earlier messages removed.]"});
      for (auto it = context.end() - keep; it != context.end(); ++it) {
        compacted.push_back(*it);
      }
      context = std::move(compacted);
    }
  }

  result.messages = std::move(context);
  result.estimated_tokens = ContextPruner::EstimateTokens(result.messages);
  return result;
}

std::vector<Message>
DefaultContextEngine::CompactOverflow(const std::vector<Message>& messages,
                                      const std::string& system_prompt,
                                      int keep_recent) {
  const bool use_llm =
      static_cast<bool>(summary_fn_) &&
      (config_.compaction.strategy ==
           CompactionRuntimeConfig::Strategy::kSummarize ||
       config_.compaction.strategy ==
           CompactionRuntimeConfig::Strategy::kMultistage);

  auto truncate_simple = [&]() -> std::vector<Message> {
    int keep =
        std::max(2, keep_recent > 0 ? keep_recent
                                    : static_cast<int>(messages.size()) / 2);
    std::vector<Message> compacted;
    if (!system_prompt.empty()) {
      compacted.push_back(Message{"system", system_prompt});
    }
    compacted.push_back(Message{
        "system", "[Context overflow recovery: older messages removed.]"});
    for (auto it =
             messages.end() - std::min(keep, static_cast<int>(messages.size()));
         it != messages.end(); ++it) {
      compacted.push_back(*it);
    }
    return compacted;
  };

  // Strip leading system messages before summarization to avoid
  // duplicating the system prompt in the output.
  std::vector<Message> non_system;
  for (const auto& m : messages) {
    if (m.role != "system") {
      non_system.push_back(m);
    }
  }

  // If we have a summary function and strategy allows LLM compaction, try
  // multi-stage compaction for larger histories.
  if (use_llm && static_cast<int>(non_system.size()) >=
                     config_.compaction.min_messages_for_multistage) {
    CompactionOptions opts;
    opts.target_tokens =
        config_.compaction.target_tokens > 0
            ? config_.compaction.target_tokens
            : config_.context_window / 4;
    opts.max_chunk_tokens = config_.compaction.max_chunk_tokens;
    opts.safety_margin = config_.compaction.safety_margin;
    opts.min_messages_for_multistage =
        config_.compaction.min_messages_for_multistage;

    std::vector<Message> result;
    if (!system_prompt.empty()) {
      result.push_back(Message{"system", system_prompt});
    }
    try {
      auto summary =
          compactor_.CompactMultiStage(non_system, opts, summary_fn_);
      for (auto& m : summary) {
        result.push_back(std::move(m));
      }
    } catch (const std::exception& e) {
      logger_->error("Multi-stage compaction failed: {}", e.what());
      return truncate_simple();
    }
    // Keep recent messages after the summary for continuity
    int keep = std::max(
        2, keep_recent > 0 ? keep_recent
                           : std::min(4, static_cast<int>(messages.size())));
    int msg_count = static_cast<int>(messages.size());
    for (auto it = messages.end() - std::min(keep, msg_count);
         it != messages.end(); ++it) {
      result.push_back(*it);
    }
    return result;
  }

  // Fallback: simple truncation (fast path, no LLM call needed)
  return truncate_simple();
}

}  // namespace quantclaw
