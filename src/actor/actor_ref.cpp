#include <oxherdcpp/actor/actor_ref.h>

#include <oxherdcpp/actor/actor_registry.h>
#include <oxherdcpp/actor/actor_system_facade.h>

namespace oxherdcpp
{

ActorRef::ActorRef(const ActorId actor_id, Wptr<ActorSystemFacade> system_facade)
    : actor_id_{actor_id}, system_facade_{std::move(system_facade)}
{
}

ActorRef::ActorRef(const Sptr<Actor> &actor, Wptr<ActorSystemFacade> system_facade)
    : actor_id_{actor->GetId()}, system_facade_{std::move(system_facade)}, cached_actor_{actor}
{
}

auto ActorRef::Tell(MPtr<BaseMessage> message) noexcept -> void
{
    if (const auto actor{cached_actor_.lock()})
    {
        actor->Receive(std::move(message));
        return;
    }
    if (const auto facade{system_facade_.lock()})
    {
        auto cb{[this, msg = std::move(message)](ActorRef ref) {
            ref.Tell(msg);
            cached_actor_ = ref.cached_actor_;
        }};
        auto msg{MakeMessage<FindActorWithCallbackMessage>()};
        msg->actor_id = actor_id_;
        msg->callback = std::move(cb);
        facade->GetActorRegistry().Tell(std::move(msg));
    }
    else
    {
        // TODO Dispatch to DeadLetters queue
    }
}

ActorRef::operator bool() const noexcept
{
    return !cached_actor_.expired();
}
} // namespace oxherdcpp