#pragma once

#include <string_view>

namespace oxherdcpp
{
template <typename T> constexpr auto GetTypeHash() -> std::size_t
{
    constexpr std::string_view func_name = __PRETTY_FUNCTION__;
    return std::hash<std::string_view>{}(func_name);
}
} // namespace oxherdcpp