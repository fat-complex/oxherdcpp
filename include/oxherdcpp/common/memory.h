#pragma once

#include <memory>

namespace oxherdcpp
{

template <typename T> using Uptr = std::unique_ptr<T>;

template <typename T, typename... Args> auto MakeUptr(Args &&...args) -> Uptr<T>
{
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template <typename T> using Sptr = std::shared_ptr<T>;

template <typename T, typename... Args> auto MakeSptr(Args &&...args) -> Sptr<T>
{
    return std::make_shared<T>(std::forward<Args>(args)...);
}

template <typename T> using Wptr = std::weak_ptr<T>;

} // namespace oxherdcpp
