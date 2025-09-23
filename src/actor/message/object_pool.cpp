#include <oxherdcpp/actor/message/object_pool.h>

namespace oxherdcpp
{

MonitoredPoolResource::MonitoredPoolResource(memory_resource *upstream) : synchronized_pool_resource{upstream}
{
}

PoolStats &MonitoredPoolResource::GetStats()
{
    return stats_;
}

const PoolStats &MonitoredPoolResource::GetStats() const
{
    return stats_;
}

void *MonitoredPoolResource::do_allocate(const std::size_t bytes, const std::size_t alignment)
{
    void *ptr{synchronized_pool_resource::do_allocate(bytes, alignment)};
    stats_.allocations.fetch_add(1, std::memory_order_relaxed);
    stats_.bytes_allocated.fetch_add(bytes, std::memory_order_relaxed);
    return ptr;
}

void MonitoredPoolResource::do_deallocate(void *ptr, const std::size_t bytes, const std::size_t alignment)
{
    synchronized_pool_resource::do_deallocate(ptr, bytes, alignment);
    stats_.deallocations.fetch_add(1, std::memory_order_relaxed);
    stats_.bytes_deallocated.fetch_add(bytes, std::memory_order_relaxed);
}
} // namespace oxherdcpp