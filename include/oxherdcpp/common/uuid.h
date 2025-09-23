#pragma once

#include <atomic>
#include <cstddef>

namespace oxherdcpp
{
template <typename Tag, typename IdType> class IDGenerator;

template <typename Tag> class IDGenerator<Tag, std::size_t>
{
  public:
    using IdType = std::size_t;

    static auto Generate()
    {
        return id_generator_++;
    }

  private:
    inline static std::atomic<std::size_t> id_generator_{0};
};
} // namespace oxherdcpp
