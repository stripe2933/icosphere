//
// Created by gomkyung2 on 2023/09/05.
//

#pragma once

#include <variant>

#include <imgui.h>

#define IMGUI_VARIANT_SELECTOR_FWD(x) std::forward<decltype(x)>(x)

template <typename>
struct ImGuiLabel { };

#define IMGUI_LABEL(type, label) \
    template <> \
    struct ImGuiLabel<type> { \
        static constexpr const char *value = label; \
    }

namespace ImGui::VariantSelector{
    namespace details{
        // https://www.walletfox.com/course/patternmatchingcpp17.php
        template<class... Ts>
        struct overload : Ts... {
            using Ts::operator()...;
        };

        template<class... Ts>
        overload(Ts...) -> overload<Ts...>;

        constexpr bool all_same_value(auto &&arg, auto &&...args){
            return ((arg == args ) && ...);
        }

        template <typename T>
        void single_radio(auto &variant, auto &&args){
            auto [select_function, default_function] = args;

            if (bool selected = std::holds_alternative<T>(variant);
                ImGui::RadioButton(ImGuiLabel<T>::value, selected) && !selected)
            {
                variant = std::invoke(IMGUI_VARIANT_SELECTOR_FWD(default_function));
            }
            if (T *value = std::get_if<T>(&variant)){
                std::invoke(IMGUI_VARIANT_SELECTOR_FWD(select_function), *value);
            }
        }

        template <typename T, typename Default>
        void single_combo(auto &variant, Default &&default_function){
            if (bool selected = std::holds_alternative<T>(variant);
                ImGui::Selectable(ImGuiLabel<T>::value, selected) && !selected)
            {
                variant = std::invoke(IMGUI_VARIANT_SELECTOR_FWD(default_function));
            }
        }
    }

    template <typename... Ts, typename... Fs, typename... Defaults>
    void radio(std::variant<Ts...> &variant, std::pair<Fs, Defaults> &&...args)
        requires (details::all_same_value(sizeof...(Ts), sizeof...(Fs), sizeof...(Defaults)) && // Arguments count must be same.
                 std::conjunction_v<std::is_invocable_r<void, Fs, Ts&>...> && // select function can be invoked with variant's value.
                 std::conjunction_v<std::is_convertible<std::invoke_result_t<Defaults>, Ts>...>) // Defaults function should generate Ts-convertible type.
    {
        (details::single_radio<Ts>(variant, IMGUI_VARIANT_SELECTOR_FWD(args)), ...);
    }

    template <typename... Ts, typename... Fs, typename... Defaults>
    void combo(const char *label, std::variant<std::monostate, Ts...> &variant, const char *placeholder, std::pair<Fs, Defaults> &&...args)
        requires (details::all_same_value(sizeof...(Ts), sizeof...(Fs), sizeof...(Defaults)) && // Arguments count must be same.
                 std::conjunction_v<std::is_invocable_r<void, Fs, Ts&>...> && // select function can be invoked with variant's value.
                 std::conjunction_v<std::is_convertible<std::invoke_result_t<Defaults>, Ts>...>) // Defaults function should generate Ts-convertible type.
    {
        const char *preview_value = std::visit(details::overload {
            [](const Ts &) { return ImGuiLabel<Ts>::value; }...,
            [=](std::monostate) { return placeholder; }
        }, variant);

        if (ImGui::BeginCombo(label, preview_value)){
            (details::single_combo<Ts>(variant, IMGUI_VARIANT_SELECTOR_FWD(args.second)), ...);
            ImGui::EndCombo();
        }
        std::visit(details::overload {
            std::forward<Fs>(args.first)...,
            [](std::monostate) { }
        }, variant);
    }
}