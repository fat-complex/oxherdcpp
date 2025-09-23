#pragma once

#include <boost/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

#include <oxherdcpp/actor/message/message_id_generator.h>
#include <oxherdcpp/actor/message/object_pool.h>
#include <oxherdcpp/common/types.h>

namespace oxherdcpp
{

class BaseMessage : public boost::intrusive_ref_counter<BaseMessage>
{
  public:
    virtual ~BaseMessage() = default;
    [[nodiscard]] virtual auto GetTypeId() const -> MessageTypeID = 0;

    template <typename T> [[nodiscard]] auto IsA() const -> bool
    {
        return GetTypeId() == GetTypeHash<T>();
    }
};

template <typename Derived> class Message : public BaseMessage
{
  public:
    using Type = Derived;

    static constexpr auto GetClassTypeId() -> MessageTypeID
    {
        return GetTypeHash<Derived>();
    }

    static auto GetPoolStats() -> PoolStats &
    {
        return pool_.GetStats();
    }

    static auto ReleasePool() -> void
    {
        pool_.release();
    }

    [[nodiscard]] constexpr auto GetTypeId() const -> MessageTypeID override
    {
        return GetClassTypeId();
    }

    static void *operator new(std::size_t size)
    {
#ifndef NDEBUG
        static_assert(std::is_final_v<Derived>, "Message types that use the default pool should be 'final' for safety. "
                                                "Use custom operator new/delete for non-final types.");
#endif

        if (size != sizeof(Derived))
        {
            return ::operator new(size);
        }
        return pool_.allocate(size, alignof(Derived));
    }

    static void operator delete(void *ptr, std::size_t size) noexcept
    {
        if (ptr == nullptr)
        {
            return;
        }
        if (size != sizeof(Derived))
        {
            ::operator delete(ptr, size);
            return;
        }
        pool_.deallocate(ptr, size, alignof(Derived));
    }

    static void operator delete(void *ptr) noexcept
    {
        if (ptr == nullptr)
        {
            return;
        }
        pool_.deallocate(ptr, sizeof(Derived), alignof(Derived));
    }

  protected:
    Message() = default;
    ~Message() override = default;

  private:
    inline static MonitoredPoolResource pool_{};
};

template <typename T> using MPtr = boost::intrusive_ptr<T>;

using BaseMessagePtr = MPtr<BaseMessage>;

template <typename T, typename... Args> auto MakeMessage(Args &&...args) -> MPtr<T>
{
    static_assert(std::is_base_of_v<Message<T>, T>, "T must derive from Message<T> for pool allocation");
    return MPtr<T>(new T(std::forward<Args>(args)...));
}

template <typename T> const PoolStats &GetMessagePoolStats()
{
    return T::GetPoolStats();
}

template <typename T> void ReleaseMessagePoolMemory()
{
    T::ReleasePool();
}

template <typename... MessageTypes> void ReleaseAllMessagePools()
{
    (ReleaseMessagePoolMemory<MessageTypes>(), ...);
}

template <typename T, typename U> [[nodiscard]] auto Cast(const MPtr<U> &msg) -> MPtr<T>
{
    using NakedT = std::remove_const_t<T>;
    static_assert(std::is_base_of_v<BaseMessage, U>, "U must be derived from BaseMessage");
    static_assert(std::is_base_of_v<BaseMessage, NakedT>, "T must be derived from BaseMessage");
    static_assert(!std::is_const_v<U> || std::is_const_v<T>, "Cannot cast away const");

    if (msg && msg->template IsA<NakedT>())
    {
        if constexpr (std::is_const_v<U>)
        {
            return boost::static_pointer_cast<T>(boost::static_pointer_cast<const BaseMessage>(msg));
        }
        else
        {
            return boost::static_pointer_cast<T>(boost::static_pointer_cast<BaseMessage>(msg));
        }
    }

    return nullptr;
}

} // namespace oxherdcpp