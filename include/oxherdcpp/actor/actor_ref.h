#pragma once

#include <oxherdcpp/actor/actor_id_generator.h>
#include <oxherdcpp/actor/message/message.h>
#include <oxherdcpp/common/memory.h>

namespace oxherdcpp
{

class ActorSystemFacade;
class Actor;
class BaseMessage;

class ActorRef
{
  public:
    ActorRef(ActorId actor_id, Wptr<ActorSystemFacade> system_facade);
    ActorRef(const Sptr<Actor> &actor, Wptr<ActorSystemFacade> system_facade);

    auto Tell(MPtr<BaseMessage> message) noexcept -> void;

    explicit operator bool() const noexcept;

  private:
    ActorId actor_id_;
    Wptr<ActorSystemFacade> system_facade_;
    Wptr<Actor> cached_actor_;
};

} // namespace oxherdcpp