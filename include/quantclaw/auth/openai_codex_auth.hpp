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

namespace quantclaw::auth {

struct OpenAICodexAuthRecord {
  std::string provider = "openai-codex";
  std::string access_token;
  std::string refresh_token;
  std::string token_type = "Bearer";
  std::string scope;
  std::string account_id;
  std::string email;
  std::int64_t expires_at = 0;

  bool HasUsableAccessToken(std::int64_t now_epoch_seconds,
                            int leeway_seconds = 60) const;
  bool CanRefresh() const;
};

class OpenAICodexAuthStore {
 public:
  explicit OpenAICodexAuthStore(std::filesystem::path path = DefaultPath());

  static std::filesystem::path DefaultPath();

  const std::filesystem::path& path() const {
    return path_;
  }

  bool Exists() const;
  std::optional<OpenAICodexAuthRecord> Load() const;
  void Save(const OpenAICodexAuthRecord& record) const;
  bool Clear() const;

 private:
  std::filesystem::path path_;
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

class BearerTokenSource {
 public:
  virtual ~BearerTokenSource() = default;
  virtual std::string ResolveAccessToken() = 0;
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
