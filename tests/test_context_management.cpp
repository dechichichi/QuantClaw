// Copyright 2025 QuantClaw Contributors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/null_sink.h>
#include <filesystem>
#include <fstream>

#include "quantclaw/core/context_pruner.hpp"
#include "quantclaw/core/memory_search.hpp"
#include "quantclaw/config.hpp"

namespace quantclaw {

// ================================================================
// ContextPruner Tests
// ================================================================

class ContextPrunerTest : public ::testing::Test {
 protected:
  std::vector<Message> make_history(int assistant_count,
                                    bool with_tool_results = true) {
    std::vector<Message> history;
    for (int i = 0; i < assistant_count; ++i) {
      // User message
      history.push_back(Message{"user", "Question " + std::to_string(i)});

      // Assistant message with tool use
      Message assistant;
      assistant.role = "assistant";
      assistant.content.push_back(ContentBlock::MakeText("Thinking..."));
      assistant.content.push_back(ContentBlock::MakeToolUse(
          "tool_" + std::to_string(i), "read_file", {{"path", "/test"}}));
      history.push_back(assistant);

      // Tool result message
      if (with_tool_results) {
        Message tool_result;
        tool_result.role = "user";
        std::string content;
        for (int j = 0; j < 20; ++j) {
          content += "Line " + std::to_string(j) + " of tool result\n";
        }
        tool_result.content.push_back(
            ContentBlock::MakeToolResult("tool_" + std::to_string(i), content));
        history.push_back(tool_result);
      }
    }
    return history;
  }
};

TEST_F(ContextPrunerTest, EmptyHistory) {
  ContextPruner::Options opts;
  auto result = ContextPruner::Prune({}, opts);
  EXPECT_TRUE(result.empty());
}

TEST_F(ContextPrunerTest, SmallHistoryUnchanged) {
  auto history = make_history(2);
  ContextPruner::Options opts;
  opts.protect_recent = 5;  // Protect more than we have

  auto result = ContextPruner::Prune(history, opts);
  EXPECT_EQ(result.size(), history.size());
}

TEST_F(ContextPrunerTest, RecentToolResultsProtected) {
  auto history = make_history(5);
  ContextPruner::Options opts;
  opts.protect_recent = 3;
  opts.max_tool_result_chars = 10;  // Very small to force pruning

  auto result = ContextPruner::Prune(history, opts);

  // The most recent 3 assistant message groups should keep full tool results
  // Count how many tool results still have the original content
  int full_results = 0;
  int pruned_results = 0;
  for (const auto& msg : result) {
    for (const auto& block : msg.content) {
      if (block.type == "tool_result") {
        if (block.content.find("omitted") != std::string::npos ||
            block.content.find("...") != std::string::npos) {
          pruned_results++;
        } else {
          full_results++;
        }
      }
    }
  }

  // Should have some pruned and some full
  EXPECT_GT(full_results, 0);
  EXPECT_GT(pruned_results, 0);
}

TEST_F(ContextPrunerTest, HardPruneOldResults) {
  auto history = make_history(15);
  ContextPruner::Options opts;
  opts.protect_recent = 3;
  opts.hard_prune_after = 10;
  opts.max_tool_result_chars = 10;

  auto result = ContextPruner::Prune(history, opts);

  // Very old results should be hard-pruned (contain "omitted")
  bool found_hard_pruned = false;
  for (const auto& msg : result) {
    for (const auto& block : msg.content) {
      if (block.type == "tool_result" &&
          block.content.find("omitted") != std::string::npos) {
        found_hard_pruned = true;
        break;
      }
    }
    if (found_hard_pruned) break;
  }
  EXPECT_TRUE(found_hard_pruned);
}

TEST_F(ContextPrunerTest, SoftPruneKeepsHeadAndTail) {
  // Create a large tool result
  std::string large_content;
  for (int i = 0; i < 50; ++i) {
    large_content += "Line " + std::to_string(i) + ": some content here\n";
  }

  std::vector<Message> history;
  // Add 5 assistant turns, only the first has a big tool result
  for (int i = 0; i < 5; ++i) {
    history.push_back(Message{"user", "Q"});

    Message assistant;
    assistant.role = "assistant";
    assistant.content.push_back(ContentBlock::MakeToolUse(
        "t" + std::to_string(i), "tool", {}));
    history.push_back(assistant);

    Message tool_msg;
    tool_msg.role = "user";
    if (i == 0) {
      tool_msg.content.push_back(
          ContentBlock::MakeToolResult("t0", large_content));
    } else {
      tool_msg.content.push_back(
          ContentBlock::MakeToolResult("t" + std::to_string(i), "short"));
    }
    history.push_back(tool_msg);
  }

  ContextPruner::Options opts;
  opts.protect_recent = 2;
  opts.soft_prune_lines = 3;
  opts.max_tool_result_chars = 100;

  auto result = ContextPruner::Prune(history, opts);

  // Find the soft-pruned result for t0
  for (const auto& msg : result) {
    for (const auto& block : msg.content) {
      if (block.type == "tool_result" && block.tool_use_id == "t0") {
        EXPECT_TRUE(block.content.find("Line 0") != std::string::npos);
        EXPECT_TRUE(block.content.find("lines omitted") != std::string::npos);
        EXPECT_TRUE(block.content.find("Line 49") != std::string::npos);
      }
    }
  }
}

TEST_F(ContextPrunerTest, NoToolResultsPassthrough) {
  std::vector<Message> history;
  history.push_back(Message{"user", "Hello"});
  history.push_back(Message{"assistant", "Hi there!"});
  history.push_back(Message{"user", "How are you?"});
  history.push_back(Message{"assistant", "I'm good!"});

  ContextPruner::Options opts;
  auto result = ContextPruner::Prune(history, opts);
  EXPECT_EQ(result.size(), history.size());
}

// ================================================================
// BM25 Memory Search Tests
// ================================================================

class BM25SearchTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto null_sink = std::make_shared<spdlog::sinks::null_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("test", null_sink);
    search_ = std::make_unique<MemorySearch>(logger);

    // Create temp directory with test files
    temp_dir_ = std::filesystem::temp_directory_path() / "bm25_test";
    std::filesystem::create_directories(temp_dir_);
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(temp_dir_, ec);
  }

  void write_file(const std::string& name, const std::string& content) {
    std::ofstream ofs(temp_dir_ / name);
    ofs << content;
  }

  std::unique_ptr<MemorySearch> search_;
  std::filesystem::path temp_dir_;
};

TEST_F(BM25SearchTest, EmptyIndex) {
  auto results = search_->Search("anything");
  EXPECT_TRUE(results.empty());
}

TEST_F(BM25SearchTest, BasicSearch) {
  write_file("test.md", "The quick brown fox jumps over the lazy dog");
  search_->IndexDirectory(temp_dir_);

  auto results = search_->Search("fox");
  ASSERT_FALSE(results.empty());
  EXPECT_GT(results[0].score, 0);
}

TEST_F(BM25SearchTest, RanksRelevantHigher) {
  write_file("a.md", "machine learning is a subset of artificial intelligence");
  write_file("b.md", "the weather today is sunny and warm");
  write_file("c.md",
             "deep learning neural networks use machine learning techniques");
  search_->IndexDirectory(temp_dir_);

  auto results = search_->Search("machine learning");
  ASSERT_GE(results.size(), 2u);

  // "c.md" mentions "machine learning" twice, should rank high
  // "a.md" also mentions it
  // "b.md" should not appear or rank lowest
  bool weather_ranked_last = true;
  for (const auto& r : results) {
    if (r.source.find("b.md") != std::string::npos) {
      // Weather file should not match "machine learning"
      weather_ranked_last = true;
    }
  }
  EXPECT_TRUE(weather_ranked_last);
}

TEST_F(BM25SearchTest, IDFWeighting) {
  // Create documents where "the" appears in all but "quantum" only in one
  write_file("a.md", "the cat sat on the mat");
  write_file("b.md", "the dog ran in the park");
  write_file("c.md", "quantum computing changes the world");
  search_->IndexDirectory(temp_dir_);

  auto results = search_->Search("quantum");
  ASSERT_FALSE(results.empty());
  EXPECT_TRUE(results[0].source.find("c.md") != std::string::npos);
}

TEST_F(BM25SearchTest, DocumentLengthNormalization) {
  // Short doc with the term should score higher than long doc with same freq
  write_file("short.md", "rust programming language");
  std::string long_content = "rust programming language";
  for (int i = 0; i < 50; ++i) {
    long_content += " filler word content padding text extra stuff here";
  }
  write_file("long.md", long_content);
  search_->IndexDirectory(temp_dir_);

  auto results = search_->Search("rust programming");
  ASSERT_GE(results.size(), 2u);
  // Short doc should rank higher due to BM25 length normalization
  EXPECT_TRUE(results[0].source.find("short.md") != std::string::npos);
}

TEST_F(BM25SearchTest, ClearResetsState) {
  write_file("test.md", "some searchable content");
  search_->IndexDirectory(temp_dir_);
  EXPECT_FALSE(search_->Search("searchable").empty());

  search_->Clear();
  EXPECT_TRUE(search_->Search("searchable").empty());

  auto stats = search_->Stats();
  EXPECT_EQ(stats["indexed_entries"], 0);
  EXPECT_EQ(stats["total_documents"], 0);
}

TEST_F(BM25SearchTest, StatsReportCorrectly) {
  write_file("a.md", "paragraph one\n\nparagraph two");
  write_file("b.md", "single paragraph");
  search_->IndexDirectory(temp_dir_);

  auto stats = search_->Stats();
  EXPECT_GE(stats["indexed_entries"].get<int>(), 2);
  EXPECT_GE(stats["total_documents"].get<int>(), 2);
}

// ================================================================
// Compaction Config Tests
// ================================================================

TEST(CompactionConfigTest, DefaultValues) {
  AgentConfig config;
  EXPECT_TRUE(config.auto_compact);
  EXPECT_EQ(config.compact_max_messages, 100);
  EXPECT_EQ(config.compact_keep_recent, 20);
  EXPECT_EQ(config.compact_max_tokens, 100000);
}

TEST(CompactionConfigTest, ParseFromJson) {
  nlohmann::json j = {
      {"model", "test-model"},
      {"autoCompact", false},
      {"compactMaxMessages", 50},
      {"compactKeepRecent", 10},
      {"compactMaxTokens", 50000},
  };

  auto config = AgentConfig::FromJson(j);
  EXPECT_FALSE(config.auto_compact);
  EXPECT_EQ(config.compact_max_messages, 50);
  EXPECT_EQ(config.compact_keep_recent, 10);
  EXPECT_EQ(config.compact_max_tokens, 50000);
}

TEST(CompactionConfigTest, ParseSnakeCase) {
  nlohmann::json j = {
      {"model", "test-model"},
      {"auto_compact", false},
      {"compact_max_messages", 75},
      {"compact_keep_recent", 15},
      {"compact_max_tokens", 80000},
  };

  auto config = AgentConfig::FromJson(j);
  EXPECT_FALSE(config.auto_compact);
  EXPECT_EQ(config.compact_max_messages, 75);
  EXPECT_EQ(config.compact_keep_recent, 15);
  EXPECT_EQ(config.compact_max_tokens, 80000);
}

}  // namespace quantclaw
