// Copyright 2025 QuantClaw Contributors
// SPDX-License-Identifier: Apache-2.0

#include "quantclaw/auth/provider_auth.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "quantclaw/platform/process.hpp"

namespace quantclaw::auth {

bool ProviderAuthRecord::HasUsableAccessToken(std::int64_t now_epoch_seconds,
                                              int leeway_seconds) const {
  return !access_token.empty() &&
         expires_at > (now_epoch_seconds + leeway_seconds);
}

bool ProviderAuthRecord::CanRefresh() const {
  return !refresh_token.empty();
}

ProviderAuthStore::ProviderAuthStore(std::filesystem::path path)
    : path_(std::move(path)) {}

std::filesystem::path
ProviderAuthStore::DefaultPathFor(const std::string& provider_id) {
  return std::filesystem::path(platform::home_directory()) / ".quantclaw" /
         "auth" / (provider_id + ".json");
}

bool ProviderAuthStore::Exists() const {
  return std::filesystem::exists(path_);
}

std::optional<ProviderAuthRecord> ProviderAuthStore::Load() const {
  if (!Exists()) {
    return std::nullopt;
  }

  std::ifstream in(path_);
  if (!in) {
    throw std::runtime_error("Failed to open auth store: " + path_.string());
  }

  nlohmann::json j;
  in >> j;

  ProviderAuthRecord record;
  record.provider = j.value("provider", "");
  record.access_token = j.value("accessToken", "");
  record.refresh_token = j.value("refreshToken", "");
  record.token_type = j.value("tokenType", "Bearer");
  record.scope = j.value("scope", "");
  record.account_id = j.value("accountId", "");
  record.email = j.value("email", "");
  record.expires_at = j.value("expiresAt", static_cast<std::int64_t>(0));
  return record;
}

void ProviderAuthStore::Save(const ProviderAuthRecord& record) const {
  std::filesystem::create_directories(path_.parent_path());

  nlohmann::json j = {
      {"provider", record.provider},
      {"accessToken", record.access_token},
      {"refreshToken", record.refresh_token},
      {"tokenType", record.token_type},
      {"scope", record.scope},
      {"accountId", record.account_id},
      {"email", record.email},
      {"expiresAt", record.expires_at},
  };

  std::ofstream out(path_);
  if (!out) {
    throw std::runtime_error("Failed to write auth store: " + path_.string());
  }
  out << j.dump(2) << '\n';
  out.close();

#ifndef _WIN32
  std::filesystem::permissions(path_,
                               std::filesystem::perms::owner_read |
                                   std::filesystem::perms::owner_write,
                               std::filesystem::perm_options::replace);
#endif
}

bool ProviderAuthStore::Clear() const {
  std::error_code ec;
  const bool removed = std::filesystem::remove(path_, ec);
  if (ec) {
    throw std::runtime_error("Failed to clear auth store: " + ec.message());
  }
  return removed;
}

}  // namespace quantclaw::auth
