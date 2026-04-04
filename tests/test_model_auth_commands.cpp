// Copyright 2025 QuantClaw Contributors
// SPDX-License-Identifier: Apache-2.0

#include <filesystem>
#include <memory>
#include <sstream>
#include <vector>

#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "quantclaw/auth/github_copilot_auth.hpp"
#include "quantclaw/auth/openai_codex_auth.hpp"
#include "quantclaw/cli/model_auth_commands.hpp"

#include "test_helpers.hpp"
#include <gtest/gtest.h>

namespace quantclaw::cli {
namespace {

std::shared_ptr<spdlog::logger> make_logger(const std::string& name) {
  auto sink = std::make_shared<spdlog::sinks::null_sink_mt>();
  return std::make_shared<spdlog::logger>(name, sink);
}

class FakeOAuthClient : public auth::OpenAICodexOAuthClient {
 public:
  explicit FakeOAuthClient(std::shared_ptr<spdlog::logger> logger)
      : OpenAICodexOAuthClient(std::move(logger)) {}

  auth::OpenAICodexAuthRecord login_result;
  int login_calls = 0;

  auth::OpenAICodexAuthRecord LoginInteractive(std::istream& /*in*/,
                                               std::ostream& /*out*/) override {
    ++login_calls;
    return login_result;
  }
};

class FakeGitHubCopilotLoginClient : public auth::GitHubCopilotLoginClient {
 public:
  explicit FakeGitHubCopilotLoginClient(std::shared_ptr<spdlog::logger> logger)
      : GitHubCopilotLoginClient(std::move(logger)) {}

  auth::GitHubCopilotAuthRecord login_result;
  int login_calls = 0;

  auth::GitHubCopilotAuthRecord
  LoginInteractive(std::istream& /*in*/, std::ostream& /*out*/) override {
    ++login_calls;
    return login_result;
  }
};

}  // namespace

TEST(ModelAuthCommandsTest, StatusWithoutCredentialsReportsLoggedOut) {
  const auto dir = test::MakeTestDir("model_auth_status");

  std::istringstream input;
  std::ostringstream output;
  std::ostringstream error;
  auto logger = make_logger("model-auth-status");
  auto client = std::make_shared<FakeOAuthClient>(logger);

  ModelAuthCommandContext ctx;
  ctx.logger = logger;
  ctx.openai_codex_client = client;
  ctx.openai_codex_store =
      auth::OpenAICodexAuthStore(dir / "openai-codex.json");
  ctx.in = &input;
  ctx.out = &output;
  ctx.err = &error;

  const int rc =
      HandleModelsAuthCommand({"status", "--provider", "openai-codex"}, ctx);

  EXPECT_EQ(rc, 0);
  EXPECT_NE(output.str().find("Not logged in"), std::string::npos);
}

TEST(ModelAuthCommandsTest, LoginStoresCredentials) {
  const auto dir = test::MakeTestDir("model_auth_login");

  std::istringstream input;
  std::ostringstream output;
  std::ostringstream error;
  auto logger = make_logger("model-auth-login");
  auto client = std::make_shared<FakeOAuthClient>(logger);
  client->login_result.provider = "openai-codex";
  client->login_result.access_token = "oauth-access";
  client->login_result.refresh_token = "oauth-refresh";
  client->login_result.account_id = "acct_123";
  client->login_result.expires_at = 4102444800;

  ModelAuthCommandContext ctx;
  ctx.logger = logger;
  ctx.openai_codex_client = client;
  ctx.openai_codex_store =
      auth::OpenAICodexAuthStore(dir / "openai-codex.json");
  ctx.in = &input;
  ctx.out = &output;
  ctx.err = &error;

  const int rc =
      HandleModelsAuthCommand({"login", "--provider", "openai-codex"}, ctx);

  EXPECT_EQ(rc, 0);
  EXPECT_EQ(client->login_calls, 1);
  EXPECT_NE(output.str().find("Logged in"), std::string::npos);

  auto saved = ctx.openai_codex_store.Load();
  ASSERT_TRUE(saved.has_value());
  EXPECT_EQ(saved->access_token, "oauth-access");
  EXPECT_EQ(saved->refresh_token, "oauth-refresh");
}

TEST(ModelAuthCommandsTest, LogoutClearsStoredCredentials) {
  const auto dir = test::MakeTestDir("model_auth_logout");
  const auto path = dir / "openai-codex.json";

  auth::OpenAICodexAuthStore store(path);
  auth::OpenAICodexAuthRecord record;
  record.provider = "openai-codex";
  record.access_token = "oauth-access";
  record.refresh_token = "oauth-refresh";
  record.expires_at = 4102444800;
  store.Save(record);

  std::istringstream input;
  std::ostringstream output;
  std::ostringstream error;
  auto logger = make_logger("model-auth-logout");
  auto client = std::make_shared<FakeOAuthClient>(logger);

  ModelAuthCommandContext ctx;
  ctx.logger = logger;
  ctx.openai_codex_client = client;
  ctx.openai_codex_store = store;
  ctx.in = &input;
  ctx.out = &output;
  ctx.err = &error;

  const int rc =
      HandleModelsAuthCommand({"logout", "--provider", "openai-codex"}, ctx);

  EXPECT_EQ(rc, 0);
  EXPECT_NE(output.str().find("Logged out"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(path));
}

TEST(ModelAuthCommandsTest,
     GitHubCopilotStatusWithoutCredentialsReportsLoggedOut) {
  const auto dir = test::MakeTestDir("model_auth_github_copilot_status");

  std::istringstream input;
  std::ostringstream output;
  std::ostringstream error;
  auto logger = make_logger("model-auth-github-copilot-status");
  auto client = std::make_shared<FakeGitHubCopilotLoginClient>(logger);

  ModelAuthCommandContext ctx;
  ctx.logger = logger;
  ctx.github_copilot_client = client;
  ctx.github_copilot_store =
      auth::GitHubCopilotAuthStore(dir / "github-copilot.json");
  ctx.in = &input;
  ctx.out = &output;
  ctx.err = &error;

  const int rc =
      HandleModelsAuthCommand({"status", "--provider", "github-copilot"}, ctx);

  EXPECT_EQ(rc, 0);
  EXPECT_NE(output.str().find("Not logged in"), std::string::npos);
}

TEST(ModelAuthCommandsTest, GitHubCopilotLoginStoresCredentials) {
  const auto dir = test::MakeTestDir("model_auth_github_copilot_login");

  std::istringstream input;
  std::ostringstream output;
  std::ostringstream error;
  auto logger = make_logger("model-auth-github-copilot-login");
  auto client = std::make_shared<FakeGitHubCopilotLoginClient>(logger);
  client->login_result.provider = "github-copilot";
  client->login_result.access_token = "github-access";
  client->login_result.account_id = "github-user";
  client->login_result.expires_at = 4102444800;

  ModelAuthCommandContext ctx;
  ctx.logger = logger;
  ctx.github_copilot_client = client;
  ctx.github_copilot_store =
      auth::GitHubCopilotAuthStore(dir / "github-copilot.json");
  ctx.in = &input;
  ctx.out = &output;
  ctx.err = &error;

  const int rc =
      HandleModelsAuthCommand({"login", "--provider", "github-copilot"}, ctx);

  EXPECT_EQ(rc, 0);
  EXPECT_EQ(client->login_calls, 1);
  EXPECT_NE(output.str().find("Logged in"), std::string::npos);

  auto saved = ctx.github_copilot_store.Load();
  ASSERT_TRUE(saved.has_value());
  EXPECT_EQ(saved->access_token, "github-access");
  EXPECT_EQ(saved->account_id, "github-user");
}

TEST(ModelAuthCommandsTest, GitHubCopilotLoginRequiresInteractiveTerminal) {
  const auto dir = test::MakeTestDir("model_auth_github_copilot_notty");

  std::istringstream input;
  std::ostringstream output;
  std::ostringstream error;
  auto logger = make_logger("model-auth-github-copilot-notty");
  auto client = std::make_shared<FakeGitHubCopilotLoginClient>(logger);

  ModelAuthCommandContext ctx;
  ctx.logger = logger;
  ctx.github_copilot_client = client;
  ctx.github_copilot_store =
      auth::GitHubCopilotAuthStore(dir / "github-copilot.json");
  ctx.in = &input;
  ctx.out = &output;
  ctx.err = &error;
  ctx.stdin_is_tty = false;

  const int rc =
      HandleModelsAuthCommand({"login", "--provider", "github-copilot"}, ctx);

  EXPECT_EQ(rc, 1);
  EXPECT_EQ(client->login_calls, 0);
  EXPECT_NE(error.str().find("interactive terminal"), std::string::npos);
}

TEST(ModelAuthCommandsTest, GitHubCopilotLoginAliasStoresCredentials) {
  const auto dir = test::MakeTestDir("model_auth_github_copilot_alias");

  std::istringstream input;
  std::ostringstream output;
  std::ostringstream error;
  auto logger = make_logger("model-auth-github-copilot-alias");
  auto client = std::make_shared<FakeGitHubCopilotLoginClient>(logger);
  client->login_result.provider = "github-copilot";
  client->login_result.access_token = "github-access";
  client->login_result.account_id = "github-user";
  client->login_result.expires_at = 4102444800;

  ModelAuthCommandContext ctx;
  ctx.logger = logger;
  ctx.github_copilot_client = client;
  ctx.github_copilot_store =
      auth::GitHubCopilotAuthStore(dir / "github-copilot.json");
  ctx.in = &input;
  ctx.out = &output;
  ctx.err = &error;

  const int rc = HandleModelsAuthCommand({"login-github-copilot"}, ctx);

  EXPECT_EQ(rc, 0);
  EXPECT_EQ(client->login_calls, 1);
  EXPECT_NE(output.str().find("Logged in"), std::string::npos);

  auto saved = ctx.github_copilot_store.Load();
  ASSERT_TRUE(saved.has_value());
  EXPECT_EQ(saved->access_token, "github-access");
}

TEST(ModelAuthCommandsTest, GitHubCopilotLogoutClearsStoredCredentials) {
  const auto dir = test::MakeTestDir("model_auth_github_copilot_logout");
  const auto path = dir / "github-copilot.json";

  auth::GitHubCopilotAuthStore store(path);
  auth::GitHubCopilotAuthRecord record;
  record.provider = "github-copilot";
  record.access_token = "github-access";
  record.expires_at = 4102444800;
  store.Save(record);

  std::istringstream input;
  std::ostringstream output;
  std::ostringstream error;
  auto logger = make_logger("model-auth-github-copilot-logout");
  auto client = std::make_shared<FakeGitHubCopilotLoginClient>(logger);

  ModelAuthCommandContext ctx;
  ctx.logger = logger;
  ctx.github_copilot_client = client;
  ctx.github_copilot_store = store;
  ctx.in = &input;
  ctx.out = &output;
  ctx.err = &error;

  const int rc =
      HandleModelsAuthCommand({"logout", "--provider", "github-copilot"}, ctx);

  EXPECT_EQ(rc, 0);
  EXPECT_NE(output.str().find("Logged out"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(path));
}

}  // namespace quantclaw::cli
