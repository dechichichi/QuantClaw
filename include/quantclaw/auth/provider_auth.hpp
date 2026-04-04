// Copyright 2025 QuantClaw Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace quantclaw::auth {

struct ProviderAuthRecord {
  std::string provider;
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

class ProviderAuthStore {
 public:
  explicit ProviderAuthStore(std::filesystem::path path);
  virtual ~ProviderAuthStore() = default;

  const std::filesystem::path& path() const {
    return path_;
  }

  bool Exists() const;
  std::optional<ProviderAuthRecord> Load() const;
  void Save(const ProviderAuthRecord& record) const;
  bool Clear() const;

 protected:
  static std::filesystem::path DefaultPathFor(const std::string& provider_id);

 private:
  std::filesystem::path path_;
};

class BearerTokenSource {
 public:
  virtual ~BearerTokenSource() = default;
  virtual std::string ResolveAccessToken() = 0;
};

#ifdef _WIN32
namespace detail {

using ProviderAuthReplaceFileFn = bool (*)(const std::filesystem::path& from,
                                           const std::filesystem::path& to,
                                           std::string* error);

void SetProviderAuthReplaceFileFnForTest(ProviderAuthReplaceFileFn fn);
void ResetProviderAuthReplaceFileFnForTest();

}  // namespace detail
#endif

}  // namespace quantclaw::auth
