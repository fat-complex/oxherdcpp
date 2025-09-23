#pragma once

#include <atomic>
#include <memory_resource>

namespace oxherdcpp
{
struct PoolStats
{
    std::atomic<std::size_t> allocations{};
    std::atomic<std::size_t> deallocations{};
    std::atomic<std::size_t> bytes_allocated{};
    std::atomic<std::size_t> bytes_deallocated{};
};

class MonitoredPoolResource final : public std::pmr::synchronized_pool_resource
{
  public:
    MonitoredPoolResource() = default;

    explicit MonitoredPoolResource(memory_resource *upstream);

    PoolStats &GetStats();
    const PoolStats &GetStats() const;

  protected:
    auto do_allocate(std::size_t bytes, std::size_t alignment) -> void * override;

    auto do_deallocate(void *ptr, std::size_t bytes, std::size_t alignment) -> void override;

  private:
    PoolStats stats_;
};
} // namespace oxherdcpp