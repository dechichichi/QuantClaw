// Copyright 2025 QuantClaw Contributors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "quantclaw/providers/provider_error.hpp"
#include "quantclaw/providers/cooldown_tracker.hpp"
#include "quantclaw/providers/failover_resolver.hpp"
#include "quantclaw/providers/provider_registry.hpp"
#include "quantclaw/config.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/null_sink.h>

using namespace quantclaw;

// ================================================================
// ProviderError tests
// ================================================================

TEST(ProviderErrorTest, ClassifyRateLimit) {
    EXPECT_EQ(ClassifyHttpError(429), ProviderErrorKind::kRateLimit);
}

TEST(ProviderErrorTest, ClassifyAuth) {
    EXPECT_EQ(ClassifyHttpError(401), ProviderErrorKind::kAuthError);
    EXPECT_EQ(ClassifyHttpError(403), ProviderErrorKind::kAuthError);
}

TEST(ProviderErrorTest, ClassifyBilling) {
    EXPECT_EQ(ClassifyHttpError(402), ProviderErrorKind::kBillingError);
}

TEST(ProviderErrorTest, ClassifyBillingFromBody) {
    EXPECT_EQ(ClassifyHttpError(400, R"({"error":"insufficient_credits"})"),
              ProviderErrorKind::kBillingError);
    EXPECT_EQ(ClassifyHttpError(400, R"({"error":"insufficient_quota"})"),
              ProviderErrorKind::kBillingError);
}

TEST(ProviderErrorTest, ClassifyModelNotFound) {
    EXPECT_EQ(ClassifyHttpError(404), ProviderErrorKind::kModelNotFound);
}

TEST(ProviderErrorTest, ClassifyTransient) {
    EXPECT_EQ(ClassifyHttpError(500), ProviderErrorKind::kTransient);
    EXPECT_EQ(ClassifyHttpError(502), ProviderErrorKind::kTransient);
    EXPECT_EQ(ClassifyHttpError(503), ProviderErrorKind::kTransient);
    EXPECT_EQ(ClassifyHttpError(504), ProviderErrorKind::kTransient);
}

TEST(ProviderErrorTest, ClassifyUnknown4xx) {
    EXPECT_EQ(ClassifyHttpError(400), ProviderErrorKind::kUnknown);
    EXPECT_EQ(ClassifyHttpError(422), ProviderErrorKind::kUnknown);
}

TEST(ProviderErrorTest, ErrorKindToString) {
    EXPECT_EQ(ProviderErrorKindToString(ProviderErrorKind::kRateLimit), "rate_limit");
    EXPECT_EQ(ProviderErrorKindToString(ProviderErrorKind::kAuthError), "auth_error");
    EXPECT_EQ(ProviderErrorKindToString(ProviderErrorKind::kBillingError), "billing_error");
    EXPECT_EQ(ProviderErrorKindToString(ProviderErrorKind::kTransient), "transient");
    EXPECT_EQ(ProviderErrorKindToString(ProviderErrorKind::kModelNotFound), "model_not_found");
    EXPECT_EQ(ProviderErrorKindToString(ProviderErrorKind::kTimeout), "timeout");
    EXPECT_EQ(ProviderErrorKindToString(ProviderErrorKind::kUnknown), "unknown");
}

TEST(ProviderErrorTest, ExceptionConstruction) {
    ProviderError err(ProviderErrorKind::kRateLimit, 429,
                      "Rate limited", "anthropic", "prod");
    EXPECT_EQ(err.Kind(), ProviderErrorKind::kRateLimit);
    EXPECT_EQ(err.HttpStatus(), 429);
    EXPECT_EQ(std::string(err.what()), "Rate limited");
    EXPECT_EQ(err.ProviderId(), "anthropic");
    EXPECT_EQ(err.ProfileId(), "prod");
}

TEST(ProviderErrorTest, InheritsFromRuntimeError) {
    try {
        throw ProviderError(ProviderErrorKind::kTransient, 503, "Service Unavailable");
    } catch (const std::runtime_error& e) {
        EXPECT_EQ(std::string(e.what()), "Service Unavailable");
    }
}

// ================================================================
// CooldownTracker tests
// ================================================================

TEST(CooldownTrackerTest, InitiallyNotInCooldown) {
    CooldownTracker tracker;
    EXPECT_FALSE(tracker.IsInCooldown("test-key"));
    EXPECT_EQ(tracker.FailureCount("test-key"), 0);
    EXPECT_EQ(tracker.CooldownRemaining("test-key").count(), 0);
}

TEST(CooldownTrackerTest, RecordFailureSetssCooldown) {
    CooldownTracker tracker;
    tracker.RecordFailure("key1", ProviderErrorKind::kRateLimit);

    EXPECT_TRUE(tracker.IsInCooldown("key1"));
    EXPECT_EQ(tracker.FailureCount("key1"), 1);
    EXPECT_GT(tracker.CooldownRemaining("key1").count(), 0);
}

TEST(CooldownTrackerTest, RecordSuccessClearsCooldown) {
    CooldownTracker tracker;
    tracker.RecordFailure("key1", ProviderErrorKind::kRateLimit);
    EXPECT_TRUE(tracker.IsInCooldown("key1"));

    tracker.RecordSuccess("key1");
    EXPECT_FALSE(tracker.IsInCooldown("key1"));
    EXPECT_EQ(tracker.FailureCount("key1"), 0);
}

TEST(CooldownTrackerTest, ConsecutiveFailuresIncrement) {
    CooldownTracker tracker;
    tracker.RecordFailure("key1", ProviderErrorKind::kTransient);
    EXPECT_EQ(tracker.FailureCount("key1"), 1);

    tracker.RecordFailure("key1", ProviderErrorKind::kTransient);
    EXPECT_EQ(tracker.FailureCount("key1"), 2);

    tracker.RecordFailure("key1", ProviderErrorKind::kTransient);
    EXPECT_EQ(tracker.FailureCount("key1"), 3);
}

TEST(CooldownTrackerTest, ModelNotFoundNoCooldown) {
    CooldownTracker tracker;
    tracker.RecordFailure("key1", ProviderErrorKind::kModelNotFound);

    // Model not found should have 0 cooldown
    EXPECT_FALSE(tracker.IsInCooldown("key1"));
}

TEST(CooldownTrackerTest, AuthErrorFixedCooldown) {
    CooldownTracker tracker;
    tracker.RecordFailure("key1", ProviderErrorKind::kAuthError);

    EXPECT_TRUE(tracker.IsInCooldown("key1"));
    // Auth error cooldown is fixed at 3600s
    auto remaining = tracker.CooldownRemaining("key1");
    EXPECT_GE(remaining.count(), 3590);  // Allow small margin
    EXPECT_LE(remaining.count(), 3600);
}

TEST(CooldownTrackerTest, ResetClearsAll) {
    CooldownTracker tracker;
    tracker.RecordFailure("key1", ProviderErrorKind::kRateLimit);
    tracker.RecordFailure("key2", ProviderErrorKind::kTransient);

    tracker.Reset();
    EXPECT_FALSE(tracker.IsInCooldown("key1"));
    EXPECT_FALSE(tracker.IsInCooldown("key2"));
}

TEST(CooldownTrackerTest, IndependentKeys) {
    CooldownTracker tracker;
    tracker.RecordFailure("key1", ProviderErrorKind::kRateLimit);

    EXPECT_TRUE(tracker.IsInCooldown("key1"));
    EXPECT_FALSE(tracker.IsInCooldown("key2"));
}

// ================================================================
// FailoverResolver tests
// ================================================================

// Mock LLM provider for testing
class MockFailoverLLM : public LLMProvider {
 public:
    explicit MockFailoverLLM(const std::string& name) : name_(name) {}

    ChatCompletionResponse ChatCompletion(const ChatCompletionRequest&) override {
        ChatCompletionResponse resp;
        resp.content = "mock response from " + name_;
        resp.finish_reason = "stop";
        return resp;
    }

    void ChatCompletionStream(const ChatCompletionRequest&,
                              std::function<void(const ChatCompletionResponse&)> cb) override {
        ChatCompletionResponse resp;
        resp.content = "mock";
        resp.is_stream_end = true;
        cb(resp);
    }

    std::string GetProviderName() const override { return name_; }
    std::vector<std::string> GetSupportedModels() const override { return {"mock-model"}; }

 private:
    std::string name_;
};

class FailoverResolverTest : public ::testing::Test {
 protected:
    void SetUp() override {
        auto null_sink = std::make_shared<spdlog::sinks::null_sink_mt>();
        logger_ = std::make_shared<spdlog::logger>("failover_test", null_sink);

        registry_ = std::make_unique<ProviderRegistry>(logger_);

        // Register mock factories
        registry_->RegisterFactory("anthropic", [](const ProviderEntry& entry, auto /*logger*/) {
            return std::make_shared<MockFailoverLLM>("anthropic:" + entry.api_key);
        });
        registry_->RegisterFactory("openai", [](const ProviderEntry& entry, auto /*logger*/) {
            return std::make_shared<MockFailoverLLM>("openai:" + entry.api_key);
        });

        // Add provider entries
        ProviderEntry anthropic_entry;
        anthropic_entry.id = "anthropic";
        anthropic_entry.api_key = "default-key";
        registry_->AddProvider(anthropic_entry);

        ProviderEntry openai_entry;
        openai_entry.id = "openai";
        openai_entry.api_key = "openai-key";
        registry_->AddProvider(openai_entry);

        resolver_ = std::make_unique<FailoverResolver>(registry_.get(), logger_);
    }

    std::shared_ptr<spdlog::logger> logger_;
    std::unique_ptr<ProviderRegistry> registry_;
    std::unique_ptr<FailoverResolver> resolver_;
};

TEST_F(FailoverResolverTest, BasicResolve) {
    auto result = resolver_->Resolve("anthropic/claude-sonnet-4-6");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->provider_id, "anthropic");
    EXPECT_EQ(result->model, "claude-sonnet-4-6");
    EXPECT_FALSE(result->is_fallback);
}

TEST_F(FailoverResolverTest, ResolveWithProfiles) {
    std::vector<AuthProfile> profiles = {
        {"prod", "sk-prod-key", ""},
        {"backup", "sk-backup-key", ""},
    };
    resolver_->SetProfiles("anthropic", profiles);

    auto result = resolver_->Resolve("anthropic/claude-sonnet-4-6");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->profile_id, "prod");
    EXPECT_EQ(result->provider->GetProviderName(), "anthropic:sk-prod-key");
}

TEST_F(FailoverResolverTest, ProfileRotationOnCooldown) {
    std::vector<AuthProfile> profiles = {
        {"prod", "sk-prod-key", ""},
        {"backup", "sk-backup-key", ""},
    };
    resolver_->SetProfiles("anthropic", profiles);

    // Put prod in cooldown
    resolver_->RecordFailure("anthropic", "prod", ProviderErrorKind::kRateLimit);

    // Should select backup
    auto result = resolver_->Resolve("anthropic/claude-sonnet-4-6");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->profile_id, "backup");
    EXPECT_EQ(result->provider->GetProviderName(), "anthropic:sk-backup-key");
}

TEST_F(FailoverResolverTest, FallbackChain) {
    resolver_->SetFallbackChain({"openai/gpt-4o"});

    // Put anthropic in cooldown (no profiles, so the default entry is cooled down)
    resolver_->RecordFailure("anthropic", "", ProviderErrorKind::kRateLimit);

    auto result = resolver_->Resolve("anthropic/claude-sonnet-4-6");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->provider_id, "openai");
    EXPECT_TRUE(result->is_fallback);
}

TEST_F(FailoverResolverTest, AllExhausted) {
    resolver_->SetFallbackChain({"openai/gpt-4o"});

    // Put both providers in cooldown
    resolver_->RecordFailure("anthropic", "", ProviderErrorKind::kRateLimit);
    resolver_->RecordFailure("openai", "", ProviderErrorKind::kRateLimit);

    auto result = resolver_->Resolve("anthropic/claude-sonnet-4-6");
    EXPECT_FALSE(result.has_value());
}

TEST_F(FailoverResolverTest, SessionPin) {
    std::vector<AuthProfile> profiles = {
        {"prod", "sk-prod-key", ""},
        {"backup", "sk-backup-key", ""},
    };
    resolver_->SetProfiles("anthropic", profiles);

    // First resolve: gets prod
    auto result1 = resolver_->Resolve("anthropic/claude-sonnet-4-6", "session-1");
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1->profile_id, "prod");

    // Record success to pin session
    resolver_->RecordSuccess("anthropic", "prod", "session-1");

    // Second resolve: should use pinned profile
    auto result2 = resolver_->Resolve("anthropic/claude-sonnet-4-6", "session-1");
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2->profile_id, "prod");
}

TEST_F(FailoverResolverTest, SessionPinClearedOnCooldown) {
    std::vector<AuthProfile> profiles = {
        {"prod", "sk-prod-key", ""},
        {"backup", "sk-backup-key", ""},
    };
    resolver_->SetProfiles("anthropic", profiles);

    // Pin to prod
    resolver_->RecordSuccess("anthropic", "prod", "session-1");

    // Put prod in cooldown
    resolver_->RecordFailure("anthropic", "prod", ProviderErrorKind::kRateLimit);

    // Should use backup (pin cleared because prod is in cooldown)
    auto result = resolver_->Resolve("anthropic/claude-sonnet-4-6", "session-1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->profile_id, "backup");
}

TEST_F(FailoverResolverTest, ClearSessionPin) {
    resolver_->RecordSuccess("anthropic", "default", "session-1");
    resolver_->ClearSessionPin("session-1");
    // No crash, pin is cleared
}

TEST_F(FailoverResolverTest, SuccessClearsCooldown) {
    resolver_->RecordFailure("anthropic", "", ProviderErrorKind::kRateLimit);
    EXPECT_TRUE(resolver_->GetCooldownTracker().IsInCooldown("anthropic"));

    resolver_->RecordSuccess("anthropic", "");
    EXPECT_FALSE(resolver_->GetCooldownTracker().IsInCooldown("anthropic"));
}

// ================================================================
// Config integration
// ================================================================

TEST(FailoverConfigTest, ParseFallbacks) {
    nlohmann::json j = {
        {"agent", {
            {"model", "anthropic/claude-sonnet-4-6"},
            {"fallbacks", {"openai/gpt-4o", "ollama/llama3"}}
        }}
    };
    auto config = QuantClawConfig::FromJson(j);
    ASSERT_EQ(config.agent.fallbacks.size(), 2u);
    EXPECT_EQ(config.agent.fallbacks[0], "openai/gpt-4o");
    EXPECT_EQ(config.agent.fallbacks[1], "ollama/llama3");
}

TEST(FailoverConfigTest, EmptyFallbacks) {
    auto config = QuantClawConfig::FromJson({});
    EXPECT_TRUE(config.agent.fallbacks.empty());
}
