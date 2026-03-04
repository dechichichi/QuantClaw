// Copyright 2025 QuantClaw Contributors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include "quantclaw/common/try.hpp"
#include "quantclaw/common/defer.hpp"

// ── Minimal StatusOr stand-in for QC_TRY tests ──────────────────────────────
// (The real StatusOr lives in providers; avoid pulling in that dependency.)
namespace {

template <typename T>
struct Result {
    bool ok;
    T    value;

    explicit operator bool() const noexcept { return ok; }
    T& operator*()            noexcept { return value; }
    const T& operator*() const noexcept { return value; }
};

template <typename T> Result<T> Ok(T v)    { return {true,  std::move(v)}; }
template <typename T> Result<T> Err(T def) { return {false, std::move(def)}; }

}  // namespace

// ── QC_TRY ───────────────────────────────────────────────────────────────────

static Result<int> double_if_ok(bool succeed) {
    int v = QC_TRY(succeed ? Ok(21) : Err(0));
    return Ok(v * 2);
}

TEST(QcTry, PropagatesErrorOnFailure) {
    auto r = double_if_ok(false);
    EXPECT_FALSE(static_cast<bool>(r));
}

TEST(QcTry, UnwrapsValueOnSuccess) {
    auto r = double_if_ok(true);
    ASSERT_TRUE(static_cast<bool>(r));
    EXPECT_EQ(*r, 42);
}

TEST(QcTry, ChainedCalls) {
    // Return type must match the error wrapper type for propagation to compile.
    auto chain = [](bool a, bool b) -> Result<int> {
        int x = QC_TRY(a ? Ok(10) : Err(0));
        int y = QC_TRY(b ? Ok(20) : Err(0));
        return Ok(x + y);
    };

    EXPECT_FALSE(static_cast<bool>(chain(false, true)));
    EXPECT_FALSE(static_cast<bool>(chain(true,  false)));
    auto r = chain(true, true);
    ASSERT_TRUE(static_cast<bool>(r));
    EXPECT_EQ(*r, 30);
}

// ── Defer ────────────────────────────────────────────────────────────────────

TEST(Defer, RunsOnScopeExit) {
    bool ran = false;
    {
        auto g = quantclaw::MakeDefer([&] { ran = true; });
        EXPECT_FALSE(ran);
    }
    EXPECT_TRUE(ran);
}

TEST(Defer, DismissCancels) {
    bool ran = false;
    {
        auto g = quantclaw::MakeDefer([&] { ran = true; });
        g.dismiss();
    }
    EXPECT_FALSE(ran);
}

TEST(Defer, ArmReenables) {
    bool ran = false;
    {
        auto g = quantclaw::MakeDefer([&] { ran = true; });
        g.dismiss();
        EXPECT_FALSE(g.is_active());
        g.arm();
        EXPECT_TRUE(g.is_active());
    }
    EXPECT_TRUE(ran);
}

TEST(Defer, MoveTransfersOwnership) {
    bool ran = false;
    {
        auto a = quantclaw::MakeDefer([&] { ran = true; });
        auto b = std::move(a);
        EXPECT_FALSE(a.is_active());  // NOLINT(bugprone-use-after-move)
        EXPECT_TRUE(b.is_active());
    }
    EXPECT_TRUE(ran);
}

TEST(Defer, DeferMacroRuns) {
    bool ran = false;
    {
        DEFER(ran = true);
    }
    EXPECT_TRUE(ran);
}

TEST(Defer, DeferMacroBlock) {
    int counter = 0;
    {
        DEFER({ counter++; counter++; });
    }
    EXPECT_EQ(counter, 2);
}

TEST(Defer, MultipleDefersRunInReverseOrder) {
    std::vector<int> order;
    {
        DEFER(order.push_back(1));
        DEFER(order.push_back(2));
        DEFER(order.push_back(3));
    }
    // C++ local destructors unwind in reverse declaration order
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 3);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 1);
}
