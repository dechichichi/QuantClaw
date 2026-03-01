// Copyright 2025 QuantClaw Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>
#include <vector>

#include "quantclaw/providers/llm_provider.hpp"

namespace quantclaw {

// Prunes old tool results from conversation history to reduce context size.
//
// Strategy:
// - Recent assistant messages (within protect_recent) keep their tool
//   results intact.
// - Older tool results are "soft pruned": truncated to a summary
//   showing the first/last few lines with an ellipsis marker.
// - Very old tool results (beyond hard_prune_after) are replaced
//   entirely with a placeholder.
//
// This does NOT modify the original history — it returns a new copy.
class ContextPruner {
 public:
  struct Options {
    int protect_recent = 3;      // Keep tool results for the N most recent
                                 // assistant messages intact
    int soft_prune_lines = 5;    // Keep this many lines at start+end for soft
    int hard_prune_after = 10;   // Hard prune tool results older than this
                                 // many assistant messages
    int max_tool_result_chars = 2000;  // Soft prune results exceeding this
  };

  // Prune tool results in a message history.
  // Returns a new vector with pruned content — does not modify input.
  static std::vector<Message> Prune(const std::vector<Message>& history,
                                    const Options& opts);

 private:
  // Soft-prune a tool result: keep first/last N lines with ellipsis
  static std::string soft_prune(const std::string& content, int keep_lines);
};

}  // namespace quantclaw
