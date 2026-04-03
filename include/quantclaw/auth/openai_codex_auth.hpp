// Copyright 2025 QuantClaw Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <filesystem>
#include <istream>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include <spdlog/spdlog.h>

#include "quantclaw/auth/provider_auth.hpp"

namespace quantclaw::auth {

using OpenAICodexAuthRecord = ProviderAuthRecord;

class OpenAICodexAuthStore : public ProviderAuthStore {
 public:
  explicit OpenAICodexAuthStore(std::filesystem::path path = DefaultPath());

  static std::filesystem::path DefaultPath();
};

struct OpenAICodexCallbackBindTarget {
  std::string host;
  int port = 0;
};

std::string BuildOpenAICodexAuthorizeUrl(const std::string& state,
                                         const std::string& code_challenge,
                                         const std::string& redirect_uri);
std::optional<OpenAICodexCallbackBindTarget>
ParseOpenAICodexCallbackBindTarget(std::string_view redirect_uri);
std::string ParseOpenAICodexManualCode(std::string input);

class OpenAICodexOAuthClient {
 public:
  explicit OpenAICodexOAuthClient(std::shared_ptr<spdlog::logger> logger);
  virtual ~OpenAICodexOAuthClient() = default;

  virtual OpenAICodexAuthRecord LoginInteractive(std::istream& in,
                                                 std::ostream& out);
  virtual OpenAICodexAuthRecord Refresh(const OpenAICodexAuthRecord& record);

  static std::string ExtractAccountId(const std::string& access_token);

 protected:
  OpenAICodexAuthRecord ExchangeCode(const std::string& code,
                                     const std::string& code_verifier,
                                     const std::string& redirect_uri);
  OpenAICodexAuthRecord RefreshWithToken(const std::string& refresh_token);

 private:
  std::shared_ptr<spdlog::logger> logger_;
};

class OpenAICodexCredentialResolver : public BearerTokenSource {
 public:
  OpenAICodexCredentialResolver(OpenAICodexAuthStore store,
                                std::shared_ptr<OpenAICodexOAuthClient> client,
                                std::shared_ptr<spdlog::logger> logger);

  std::string ResolveAccessToken() override;
  std::string ResolveAccessToken(std::int64_t now_epoch_seconds);

 private:
  OpenAICodexAuthStore store_;
  std::shared_ptr<OpenAICodexOAuthClient> client_;
  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace quantclaw::auth
