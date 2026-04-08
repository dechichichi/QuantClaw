// Copyright 2025 QuantClaw Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "quantclaw/providers/llm_provider.hpp"

namespace quantclaw {

class MemoryManager;

// Deterministic pre-compaction memory extractor.
// Scans messages that are about to be discarded by compaction and extracts
// durable facts (tool outcomes, user corrections, explicit "remember"
// directives) to the workspace memory via MemoryManager.
class MemoryExtractor {
 public:
  explicit MemoryExtractor(std::shared_ptr<spdlog::logger> logger);

  // Extract facts from messages about to be discarded and append them to
  // workspace memory. Returns the number of facts extracted.
  int Extract(const std::vector<Message>& discarded_messages,
              MemoryManager& memory_manager);

 private:
  struct Fact {
    std::string category;  // "tool_outcome" | "user_correction" | "remember"
    std::string content;
  };

  std::vector<Fact> scan_messages(const std::vector<Message>& messages) const;
  std::string format_facts(const std::vector<Fact>& facts) const;

  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace quantclaw
