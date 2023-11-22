//
// Created by gomkyung2 on 2023/09/09.
//

#pragma once

/**
 * The word "dirty" means it is modified, and all other things dependent on it should be updated. Dirty property holds
 * a data that can be modified, and a dirty flag that indicates whether the data is modified or not. When the data is
 * modified, \p is_dirty flag is set to \p true. User can process the data when it is modified by calling \p clean()
 * function, and the dirty flag gets \p false .
 * @tparam T Type of data to store.
 *
 * This is the basic usage of DirtyProperty.
 *
 * @code
 * DirtyProperty<int> message_count { 0 }; // is_dirty = true, data = 0
 *
 * void cleanWithMessage(DirtyProperty<int> &prop){
 *     prop.clean([](int count){
 *         std::cout << "Message count is updated with value " << count << '\n';
 *     });
 * }
 *
 * cleanWithMessage(message_count); // print "Message count is updated with value 0", is_dirty = false.
 * cleanWithMessage(message_count); // no message printed since is_dirty flag is now false.
 * message_count = message_count.value() + 1; // property value updated, is_dirty = true, data = 1.
 * cleanWithMessage(message_count); // print "Message count is updated with value 1", is_dirty = false.
 * message_count.makeDirty(); // value is not updated, but user sets dirty flag as true. is_dirty = true, data = 1.
 * cleanWithMessage(message_count); // print "Message count is updated with value 1", is_dirty = false.
 * @endcode
 *
 * If the update should be processed when multiple properties are dirty, you can use \p DirtyPropertyHelper::clean() .
 *
 * @code
 * DirtyProperty<int> prop1 { 1 }, prop2 { 2 }; // prop1: is_dirty = true, prop2: is_dirty = true.
 * DirtyPropertyHelper::clean([](int value1, int value2){
 *     // This lambda is executed with value1 = 1, value2 = 2.
 *     // After the method executed, two properties' is_dirty gets false.
 * }, prop1, prop2);
 *
 * prop1 = 3; // prop1: is_dirty = true, prop2: is_dirty = false.
 * DirtyPropertyHelper::clean([](int value1, int value2){
 *     // This lambda is executed with value1 = 3, value2 = 2, because prop1 is dirty.
 *     // After the method executed, two properties' is_dirty gets false.
 * }, prop1, prop2);
 * @endcode
 */
template <typename T>
class DirtyProperty{
    bool is_dirty;
    T data;

public:
    using value_type = T;

    explicit constexpr DirtyProperty(bool is_dirty = true) requires std::is_default_constructible_v<T>
            : data{}, is_dirty { is_dirty }
    {

    }

    template <typename U>
    explicit constexpr DirtyProperty(U &&value, bool is_dirty = true) requires std::is_convertible_v<std::remove_cvref_t<U>, T>
            : data { std::forward<U>(value) }, is_dirty { is_dirty }
    {

    }

    constexpr DirtyProperty &operator=(auto &&new_value){
        data = std::forward<decltype(new_value)>(new_value);
        is_dirty = true;
        return *this;
    }

    /**
     * @brief Get const reference of the stored value.
     * @return Const reference of the stored value.
     * @node If you want to modify the value, use assignment operator or \p mutableValue() function instead (the latter
     * has programmer's responsibility to explicitly set dirty flag when data is modified).
     */
    [[nodiscard]] constexpr const T &value() const noexcept{
        return data;
    }

    // Unsafe. User have responsibility for make property dirty when value is changed.

    /**
     * @brief Get reference of the stored value.
     * @return Reference of the stored value.
     * @note The purpose of the dirty property is to automatically set dirty flag when the value is modified. This function
     * allows user to get mutable reference of the stored value, but user have responsibility to explicitly set dirty flag
     * when the value is modified. If you don't need the mutability of the value, use \p value() function instead.
     */
    [[nodiscard]] constexpr T &mutableValue() noexcept{
        return data;
    }

    /**
     * @brief Check whether the property is dirty or not.
     * @return \p true if the property is dirty, \p false otherwise.
     */
    [[nodiscard]] constexpr bool isDirty() const noexcept{
        return is_dirty;
    }

    /**
     * @brief Set dirty flag as \p true.
     */
    constexpr void makeDirty() noexcept{
        is_dirty = true;
    }

    /**
     * Execute the given function when the property is dirty, and set dirty flag to \p false.
     * @param function Function to execute when the property is dirty. If \p is_dirty is \p false, it does not executed.
     */
    template <typename UnaryFunction> requires std::invocable<UnaryFunction, T&>
    constexpr void clean(UnaryFunction &&function){
        if (is_dirty){
            std::invoke(std::forward<UnaryFunction>(function), data);
            is_dirty = false;
        }
    }
};

namespace DirtyPropertyHelper{
    namespace details {
        template <typename>
        struct is_dirty_property : std::false_type { };

        template <typename U>
        struct is_dirty_property<DirtyProperty<U>> : std::true_type { };

        template <typename U>
        concept dirty_property = is_dirty_property<U>::value;
    }

    template <typename Function, details::dirty_property ...Props>
        requires std::is_invocable_v<Function, typename Props::value_type&...>
    static void clean(Function &&function, Props &...props){
        if ((props.isDirty() || ...)){
            std::invoke(std::forward<Function>(function), props.mutableValue()...);
            (props.makeDirty(), ...);
        }
    }
}