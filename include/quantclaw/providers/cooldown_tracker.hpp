// Copyright 2025 QuantClaw Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

#include "quantclaw/providers/provider_error.hpp"

namespace quantclaw {

// Tracks per-key cooldown state with exponential backoff.
// Keys are typically "provider_id:profile_id" or "provider_id".
class CooldownTracker {
 public:
  // Returns true if the key is currently in cooldown.
  bool IsInCooldown(const std::string& key) const;

  // Record a failure for the given key.
  // Computes the next cooldown duration based on the error kind
  // and the number of consecutive failures.
  void RecordFailure(const std::string& key, ProviderErrorKind kind);

  // Clear cooldown state for a key (e.g. after a successful request).
  void RecordSuccess(const std::string& key);

  // Get the time remaining in cooldown for a key.
  // Returns 0 if not in cooldown.
  std::chrono::seconds CooldownRemaining(const std::string& key) const;

  // Clear all cooldown state.
  void Reset();

  // Returns the number of consecutive failures for a key.
  int FailureCount(const std::string& key) const;

 private:
  struct CooldownState {
    int consecutive_failures = 0;
    ProviderErrorKind last_error = ProviderErrorKind::kUnknown;
    std::chrono::steady_clock::time_point cooldown_until;
  };

  // Compute cooldown duration based on error kind and failure count.
  static std::chrono::seconds ComputeCooldown(ProviderErrorKind kind,
                                               int failure_count);

  mutable std::mutex mu_;
  std::unordered_map<std::string, CooldownState> states_;
};

}  // namespace quantclaw
