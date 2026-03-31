// Copyright 2025 QuantClaw Contributors
// SPDX-License-Identifier: Apache-2.0

#include "quantclaw/providers/github_copilot_provider.hpp"

namespace quantclaw {

GitHubCopilotProvider::GitHubCopilotProvider(
    int timeout, std::shared_ptr<spdlog::logger> logger,
    std::shared_ptr<auth::GitHubCopilotRuntimeResolverInterface> resolver)
    : OpenAIProvider("", "https://api.individual.githubcopilot.com", timeout,
                     std::move(logger)),
      resolver_(std::move(resolver)) {}

std::string GitHubCopilotProvider::GetProviderName() const {
  return "github-copilot";
}

std::vector<std::string> GitHubCopilotProvider::GetSupportedModels() const {
  return {"gpt-4o", "gpt-4.1", "claude-3.7-sonnet", "claude-sonnet-4"};
}

std::string GitHubCopilotProvider::ResolveApiKey() const {
  return resolver_->ResolveRuntimeCredential().api_token;
}

std::string GitHubCopilotProvider::ResolveBaseUrl() const {
  const auto runtime = resolver_->ResolveRuntimeCredential();
  return runtime.base_url.empty() ? OpenAIProvider::ResolveBaseUrl()
                                  : runtime.base_url;
}

std::string GitHubCopilotProvider::ProviderId() const {
  return "github-copilot";
}

}  // namespace quantclaw
