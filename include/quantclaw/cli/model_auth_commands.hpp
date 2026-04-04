// Copyright 2025 QuantClaw Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <istream>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "quantclaw/auth/github_copilot_auth.hpp"
#include "quantclaw/auth/openai_codex_auth.hpp"

namespace quantclaw::cli {

struct ModelAuthCommandContext {
  std::shared_ptr<spdlog::logger> logger;
  std::shared_ptr<auth::OpenAICodexOAuthClient> openai_codex_client;
  auth::OpenAICodexAuthStore openai_codex_store;
  std::shared_ptr<auth::GitHubCopilotLoginClient> github_copilot_client;
  auth::GitHubCopilotAuthStore github_copilot_store;
  std::istream* in = nullptr;
  std::ostream* out = nullptr;
  std::ostream* err = nullptr;
  bool stdin_is_tty = true;
};

int HandleModelsAuthCommand(const std::vector<std::string>& args,
                            ModelAuthCommandContext ctx);

}  // namespace quantclaw::cli
