//
// Created by gomkyung2 on 2023/09/09.
//

#pragma once

// https://www.walletfox.com/course/patternmatchingcpp17.php
template<class... Ts>
struct overload : Ts... {
    using Ts::operator()...;
};

template<class... Ts>
overload(Ts...) -> overload<Ts...>;