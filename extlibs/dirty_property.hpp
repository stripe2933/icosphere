//
// Created by gomkyung2 on 2023/09/09.
//

#pragma once

template <typename T>
struct DirtyProperty{
    bool is_dirty = true;
    T value;

    template <typename U>
    DirtyProperty(U &&value) requires std::is_convertible_v<std::remove_cvref_t<U>, T>
            : value { std::forward<decltype(value)>(value) }
    {

    }
};