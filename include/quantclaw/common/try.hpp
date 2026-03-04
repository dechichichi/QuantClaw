// Copyright 2025 QuantClaw Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <type_traits>
#include <utility>

// ─── QC_TRY ────────────────────────────────────────────────────────────────
//
// Propagates errors up the call stack, similar to Rust's `?` operator.
//
// Given an expression that returns a StatusOr<T> (or any type that is
// contextually convertible to bool for "is OK" and supports operator* for
// value extraction), QC_TRY either:
//   • returns the error from the enclosing function if the result is falsy, or
//   • yields the unwrapped success value if truthy.
//
// Usage:
//   auto val  = QC_TRY(ComputeSomething(x));      // StatusOr<int> → int
//   auto data = QC_TRY(LoadFile(path));            // StatusOr<Bytes> → Bytes
//
// The macro requires GCC/Clang statement-expression support (widely available
// on all platforms QuantClaw targets).
//
// Implementation note: the internal helper qc_detail::unwrap_result<T> exists
// solely to give meaningful compiler messages when T has no operator*.

namespace quantclaw::qc_detail {

template <typename T>
struct unwrap_result {
    static decltype(auto) get(T&& v) { return *std::forward<T>(v); }
};

}  // namespace quantclaw::qc_detail

#define QC_TRY(...)                                                             \
    __extension__({                                                             \
        auto _r_ = (__VA_ARGS__);                                               \
        if (!static_cast<bool>(_r_))                                            \
            return std::forward<decltype(_r_)>(_r_);                            \
        ::quantclaw::qc_detail::unwrap_result<decltype(_r_)>::get(              \
            std::forward<decltype(_r_)>(_r_));                                  \
    })
