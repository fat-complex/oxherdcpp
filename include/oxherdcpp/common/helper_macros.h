#pragma once

namespace oxherdcpp
{

#define DEFAULT_COPY(Class)                                                                                            \
    Class(const Class &) = default;                                                                                    \
    Class &operator=(const Class &) = default;

#define DEFAULT_COPY_AND_MOVE(Class)                                                                                   \
    Class(const Class &) = default;                                                                                    \
    Class &operator=(const Class &) = default;                                                                         \
    Class(Class &&) = default;                                                                                         \
    Class &operator=(Class &&) = default;

#define DISABLE_COPY(Class)                                                                                            \
    Class(const Class &) = delete;                                                                                     \
    Class &operator=(const Class &) = delete;

#define DISABLE_COPY_AND_MOVE(Class)                                                                                   \
    Class(const Class &) = delete;                                                                                     \
    Class &operator=(const Class &) = delete;                                                                          \
    Class(Class &&) = delete;                                                                                          \
    Class &operator=(Class &&) = delete;

#define RETURN_IF(expression, action)                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        if (expression)                                                                                                \
        {                                                                                                              \
            action;                                                                                                    \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)
} // namespace oxherdcpp