// SPDX-License-Identifier: GPL-2.0-or-later
// bbport: calls of guest function pointers from the GPU library (HLE callbacks such as AvPlayer's
// file and memory replacements). With translated guest code (bbcpu, docs/ARM64_NATIVE.md) they run
// through the translator; host functions and native guest code are called directly.
#pragma once

#include <cstdint>
#include <type_traits>

extern "C" std::uint64_t runtime_guest_call(const void* fn, int count, const std::uint64_t* args);

namespace BbGuest {
template <typename T>
std::uint64_t ToArg(T value) {
    if constexpr (std::is_pointer_v<T>) {
        return reinterpret_cast<std::uintptr_t>(value);
    } else if constexpr (std::is_enum_v<T>) {
        return static_cast<std::uint64_t>(static_cast<std::underlying_type_t<T>>(value));
    } else {
        return static_cast<std::uint64_t>(value);
    }
}

/// Calls fn(args...) (integer and pointer arguments only) and returns its result as R.
template <typename R, typename F, typename... Args>
R Call(F fn, Args... args) {
    const std::uint64_t values[sizeof...(Args) ? sizeof...(Args) : 1] = {ToArg(args)...};
    const std::uint64_t result =
        runtime_guest_call(reinterpret_cast<const void*>(fn), int(sizeof...(Args)), values);
    if constexpr (std::is_void_v<R>) {
        return;
    } else if constexpr (std::is_pointer_v<R>) {
        return reinterpret_cast<R>(static_cast<std::uintptr_t>(result));
    } else {
        return static_cast<R>(result);
    }
}
} // namespace BbGuest
