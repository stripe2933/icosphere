//
// Created by gomkyung2 on 11/23/23.
//

#pragma once

#include <concepts>
#include <ratio>
#include <type_traits>
#include <chrono>
#include <functional>
#include <utility>

template <typename Unit = float, typename Ratio = std::milli, std::invocable Fn>
auto measure_execution(Fn &&func) noexcept(std::is_nothrow_invocable_v<Fn>)
    -> std::chrono::duration<Unit, Ratio>
{
    const auto start = std::chrono::high_resolution_clock::now();
    std::invoke(std::forward<Fn>(func));
    const auto end = std::chrono::high_resolution_clock::now();
    return end - start;
}

template <typename Unit = float, typename Ratio = std::milli, std::invocable Fn>
auto measure_execution_with_result(Fn &&func) noexcept(std::is_nothrow_invocable_v<Fn>)
    -> std::pair<std::invoke_result_t<Fn>, std::chrono::duration<Unit, Ratio>>
{
    const auto start = std::chrono::high_resolution_clock::now();
    auto result = std::invoke(std::forward<Fn>(func));
    const auto end = std::chrono::high_resolution_clock::now();
    return { std::move(result), end - start };
}