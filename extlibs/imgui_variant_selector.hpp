//
// Created by gomkyung2 on 2023/09/05.
//

#pragma once

#include <variant>
#include <concepts>

#include <imgui.h>

template <typename T>
struct ImGuiLabel{

};

namespace ImGui::VariantSelector{
    namespace details{
        template <typename>
        struct is_pair_t : std::false_type {};

        template <typename U, typename V>
        struct is_pair_t<std::pair<U, V>> : std::true_type {};

        template <typename T>
        concept pair_t = is_pair_t<T>::value;

        template <typename>
        struct is_variant_t : std::false_type {};

        template <typename... Ts>
        struct is_variant_t<std::variant<Ts...>> : std::true_type {};

        template <typename T>
        concept variant_t = is_variant_t<T>::value;

        // https://www.walletfox.com/course/patternmatchingcpp17.php
        template<class... Ts>
        struct overload : Ts... {
            using Ts::operator()...;
        };

        template<class... Ts>
        overload(Ts...) -> overload<Ts...>;

        template <typename T>
        void single_radio(variant_t auto &variant, pair_t auto &&args){
            auto &&[select_function, default_function] = args;

            if (bool selected = std::holds_alternative<T>(variant);
                ImGui::RadioButton(ImGuiLabel<T>::value, selected) && !selected)
            {
                variant = default_function();
            }
            if (T *value = std::get_if<T>(&variant)){
                select_function(*value);
            }
        }

        template <typename T, typename Default>
        void single_combo(variant_t auto &variant, Default &&default_function){
            if (bool selected = std::holds_alternative<T>(variant);
                ImGui::Selectable(ImGuiLabel<T>::value, selected) && !selected)
            {
                variant = default_function();
            }
        }
    }

    template <typename... Ts, typename... Fs, typename... Defaults>
    void radio(std::variant<Ts...> &variant, std::pair<Fs, Defaults> &&...args)
        requires (sizeof...(Ts) == sizeof...(Fs)) && (sizeof...(Ts) == sizeof...(Defaults)) && // Arguments count must be same.
                 (std::is_invocable_r_v<void, Fs, Ts&> && ...) && // select function can be invoked with variant's value.
                 (std::is_convertible_v<std::invoke_result_t<Defaults>, Ts> &&...) // Defaults function should generate Ts-convertible type.
    {
        (details::single_radio<Ts>(variant, std::forward<decltype(args)>(args)), ...);
    }

    template <typename... Ts, typename... Fs, typename... Defaults>
    void combo(const char *label, std::variant<std::monostate, Ts...> &variant, const char *placeholder, std::pair<Fs, Defaults> &&...args)
        requires (sizeof...(Ts) == sizeof...(Fs)) && (sizeof...(Ts) == sizeof...(Defaults)) && // Arguments count must be same.
                 (std::is_invocable_r_v<void, Fs, Ts&> && ...) && // select function can be invoked with variant's value.
                 (std::is_convertible_v<std::invoke_result_t<Defaults>, Ts> &&...) // Defaults function should generate Ts-convertible type.
    {
        const char *preview_value = std::visit(details::overload {
            [](const Ts &x) { return ImGuiLabel<Ts>::value; }...,
            [=](std::monostate) { return placeholder; }
        }, variant);

        if (ImGui::BeginCombo(label, preview_value)){
            (details::single_combo<Ts>(variant, std::forward<Defaults>(args.second)), ...);
            ImGui::EndCombo();
        }
        std::visit(details::overload {
            std::forward<Fs>(args.first)...,
            [](std::monostate) { }
        }, variant);
    }
}