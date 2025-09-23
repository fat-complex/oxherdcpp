#pragma once

#include <oxherdcpp/actor/actor_ref.h>

namespace oxherdcpp
{
class ActorSystemFacade
{
  public:
    virtual ~ActorSystemFacade() = default;

    [[nodiscard]] virtual auto GetActorRegistry() -> ActorRef = 0;

    virtual auto DispatchMessage(ActorId actor_id, MPtr<BaseMessage> message) -> void = 0;
};
} // namespace oxherdcpp