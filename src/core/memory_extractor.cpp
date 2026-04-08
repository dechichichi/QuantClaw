// Copyright 2025 QuantClaw Contributors
// SPDX-License-Identifier: Apache-2.0

#include "quantclaw/core/memory_extractor.hpp"

#include <algorithm>
#include <sstream>

#include "quantclaw/core/memory_manager.hpp"

namespace quantclaw {

MemoryExtractor::MemoryExtractor(std::shared_ptr<spdlog::logger> logger)
    : logger_(std::move(logger)) {}

int MemoryExtractor::Extract(const std::vector<Message>& discarded_messages,
                             MemoryManager& memory_manager) {
  auto facts = scan_messages(discarded_messages);
  if (facts.empty()) {
    return 0;
  }

  std::string formatted = format_facts(facts);
  memory_manager.SaveDailyMemory(formatted);
  logger_->info("MemoryExtractor: extracted {} facts from {} discarded messages",
                facts.size(), discarded_messages.size());
  return static_cast<int>(facts.size());
}

std::vector<MemoryExtractor::Fact>
MemoryExtractor::scan_messages(const std::vector<Message>& messages) const {
  std::vector<Fact> facts;

  for (const auto& msg : messages) {
    // Check structured content blocks for tool results
    for (const auto& block : msg.content) {
      if (block.type == "tool_result" && !block.content.empty()) {
        bool is_error = block.content.find("Error") == 0 ||
                        block.content.find("error:") != std::string::npos;
        std::string summary;
        if (block.content.size() > 120) {
          summary = block.content.substr(0, 120) + "...";
        } else {
          summary = block.content;
        }
        facts.push_back({"tool_outcome",
                         (is_error ? "[FAIL] " : "[OK] ") + summary});
      }
    }

    // Check plain-text content for patterns
    const std::string& text = msg.content.empty() ? msg.text() : "";
    const std::string& content =
        text.empty() ? (msg.content.empty() ? "" : msg.text()) : text;
    if (content.empty() || content.size() < 10) {
      continue;
    }

    // User corrections: "no, actually", "that's wrong", "I meant"
    if (msg.role == "user") {
      auto lower = content;
      std::transform(lower.begin(), lower.end(), lower.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      bool is_correction =
          lower.find("no, actually") != std::string::npos ||
          lower.find("that's wrong") != std::string::npos ||
          lower.find("i meant") != std::string::npos ||
          lower.find("不对") != std::string::npos ||
          lower.find("不是这样") != std::string::npos ||
          lower.find("我的意思是") != std::string::npos;
      if (is_correction) {
        std::string summary =
            content.size() > 200 ? content.substr(0, 200) + "..." : content;
        facts.push_back({"user_correction", summary});
      }

      // Explicit "remember" directives
      bool is_remember =
          lower.find("remember this") != std::string::npos ||
          lower.find("remember that") != std::string::npos ||
          lower.find("keep in mind") != std::string::npos ||
          lower.find("记住") != std::string::npos ||
          lower.find("请记住") != std::string::npos;
      if (is_remember) {
        std::string summary =
            content.size() > 300 ? content.substr(0, 300) + "..." : content;
        facts.push_back({"remember", summary});
      }
    }
  }

  return facts;
}

std::string
MemoryExtractor::format_facts(const std::vector<Fact>& facts) const {
  std::ostringstream ss;
  ss << "### Auto-extracted (pre-compaction)\n";
  for (const auto& fact : facts) {
    ss << "- [" << fact.category << "] " << fact.content << "\n";
  }
  return ss.str();
}

}  // namespace quantclaw
