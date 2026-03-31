// Copyright 2025 QuantClaw Contributors
// SPDX-License-Identifier: Apache-2.0

#include <filesystem>
#include <memory>

#ifdef _WIN32
#define test_setenv(name, value) _putenv_s(name, value)
#define test_unsetenv(name) _putenv_s(name, "")
#else
#define test_setenv(name, value) setenv(name, value, 1)
#define test_unsetenv(name) unsetenv(name)
#endif

#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "quantclaw/auth/github_copilot_auth.hpp"

#include "test_helpers.hpp"
#include <gtest/gtest.h>

namespace quantclaw::auth {
namespace {

std::shared_ptr<spdlog::logger> make_logger(const std::string& name) {
  auto sink = std::make_shared<spdlog::sinks::null_sink_mt>();
  return std::make_shared<spdlog::logger>(name, sink);
}

class FakeTokenClient : public GitHubCopilotTokenClient {
 public:
  explicit FakeTokenClient(std::shared_ptr<spdlog::logger> logger)
      : GitHubCopilotTokenClient(std::move(logger)) {}

  GitHubCopilotRuntimeCredential result;
  int exchange_calls = 0;

  GitHubCopilotRuntimeCredential
  ExchangeForApiToken(const std::string& github_token) override {
    ++exchange_calls;
    last_github_token = github_token;
    return result;
  }

  std::string last_github_token;
};

}  // namespace

TEST(GitHubCopilotAuthTest, StoreRoundTripsRecord) {
  const auto dir = test::MakeTestDir("github_copilot_auth_store");
  GitHubCopilotAuthStore store(dir / "github-copilot.json");

  GitHubCopilotAuthRecord record;
  record.provider = "github-copilot";
  record.access_token = "github-access";
  record.account_id = "github-user";
  record.expires_at = 4102444800;

  store.Save(record);
  const auto loaded = store.Load();
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->provider, "github-copilot");
  EXPECT_EQ(loaded->access_token, "github-access");
  EXPECT_EQ(loaded->account_id, "github-user");
}

TEST(GitHubCopilotAuthTest, RuntimeResolverCachesUsableApiToken) {
  const auto dir = test::MakeTestDir("github_copilot_runtime_cache");
  GitHubCopilotAuthStore store(dir / "github-copilot.json");

  GitHubCopilotAuthRecord record;
  record.provider = "github-copilot";
  record.access_token = "github-access";
  record.expires_at = 4102444800;
  store.Save(record);

  auto logger = make_logger("github-copilot-runtime-cache");
  auto client = std::make_shared<FakeTokenClient>(logger);
  client->result.api_token = "copilot-api-token";
  client->result.base_url = "https://api.individual.githubcopilot.com";
  client->result.expires_at = 4102444800;

  GitHubCopilotRuntimeResolver resolver(
      store, GitHubCopilotTokenCache(dir / "github-copilot-runtime.json"),
      client, logger);

  const auto first = resolver.ResolveRuntimeCredential(4000000000);
  const auto second = resolver.ResolveRuntimeCredential(4000000001);

  EXPECT_EQ(first.api_token, "copilot-api-token");
  EXPECT_EQ(first.base_url, "https://api.individual.githubcopilot.com");
  EXPECT_EQ(second.api_token, "copilot-api-token");
  EXPECT_EQ(client->exchange_calls, 1);
  EXPECT_EQ(client->last_github_token, "github-access");
}

TEST(GitHubCopilotAuthTest, RuntimeResolverPrefersEnvironmentTokens) {
  test_unsetenv("COPILOT_GITHUB_TOKEN");
  test_unsetenv("GH_TOKEN");
  test_unsetenv("GITHUB_TOKEN");
  ASSERT_EQ(test_setenv("GITHUB_TOKEN", "from-github-token"), 0);
  ASSERT_EQ(test_setenv("GH_TOKEN", "from-gh-token"), 0);
  ASSERT_EQ(test_setenv("COPILOT_GITHUB_TOKEN", "from-copilot-token"), 0);

  const auto dir = test::MakeTestDir("github_copilot_runtime_env");
  GitHubCopilotAuthStore store(dir / "github-copilot.json");
  auto logger = make_logger("github-copilot-runtime-env");
  auto client = std::make_shared<FakeTokenClient>(logger);
  client->result.api_token = "copilot-api-token";
  client->result.base_url = "https://api.individual.githubcopilot.com";
  client->result.expires_at = 4102444800;

  GitHubCopilotRuntimeResolver resolver(
      store, GitHubCopilotTokenCache(dir / "github-copilot-runtime.json"),
      client, logger);

  const auto result = resolver.ResolveRuntimeCredential(4000000000);

  EXPECT_EQ(result.api_token, "copilot-api-token");
  EXPECT_EQ(client->exchange_calls, 1);
  EXPECT_EQ(client->last_github_token, "from-copilot-token");

  test_unsetenv("COPILOT_GITHUB_TOKEN");
  test_unsetenv("GH_TOKEN");
  test_unsetenv("GITHUB_TOKEN");
}

}  // namespace quantclaw::auth
