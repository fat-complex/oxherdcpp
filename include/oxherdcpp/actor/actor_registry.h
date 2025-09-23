#pragma once

#include <unordered_map>

#include <oxherdcpp/actor/actor.h>
#include <oxherdcpp/actor/actor_ref.h>

namespace oxherdcpp
{

struct RegisterActorMessage final : Message<RegisterActorMessage>
{
    RegisterActorMessage(const ActorId actor_id, ActorRef actor_ref)
        : actor_id{actor_id}, actor_ref{std::move(actor_ref)}
    {
    }
    ActorId actor_id;
    ActorRef actor_ref;
};

struct UnregisterActorMessage final : Message<UnregisterActorMessage>
{
    explicit UnregisterActorMessage(const ActorId actor_id) : actor_id{actor_id}
    {
    }
    ActorId actor_id;
};

struct FindActorMessage final : Message<FindActorMessage>
{
    FindActorMessage(const ActorId actor_id, ActorRef reply_to) : actor_id{actor_id}, reply_to{std::move(reply_to)}
    {
    }
    ActorId actor_id;
    ActorRef reply_to;
};

struct FindActorWithCallbackMessage final : Message<FindActorWithCallbackMessage>
{
    ActorId actor_id;
    std::function<void(ActorRef)> callback;
};

struct ActorFoundMessage final : Message<ActorFoundMessage>
{
    ActorFoundMessage(const ActorId actor_id, ActorRef actor_ref) : actor_id{actor_id}, actor_ref_(std::move(actor_ref))
    {
    }
    ActorId actor_id;
    ActorRef actor_ref_;
};

struct ActorFoundResponseMessage final : Message<ActorFoundResponseMessage>
{
    explicit ActorFoundResponseMessage(ActorRef actor_ref) : actor_ref{std::move(actor_ref)}
    {
    }
    ActorRef actor_ref;
};

struct ActorNotFoundResponseMessage final : Message<ActorNotFoundResponseMessage>
{
    explicit ActorNotFoundResponseMessage(const ActorId actor_id) : actor_id{actor_id}
    {
    }
    ActorId actor_id;
    std::uint64_t request_id{};
};

struct ActorNotFoundMessage final : Message<ActorNotFoundMessage>
{
    explicit ActorNotFoundMessage(const ActorId actor_id) : actor_id{actor_id}
    {
    }
    ActorId actor_id;
};

class ActorRegistry final : public Actor
{
  public:
    explicit ActorRegistry(const Executor &executor, const std::string &name, ActorId actor_id);

  private:
    auto Behaviour(const MPtr<BaseMessage> &message) -> void override;

    auto HandleRegisterActor(const MPtr<RegisterActorMessage> &message) -> void;

    auto HandleUnregisterActor(const MPtr<UnregisterActorMessage> &message) -> void;

    auto HandleFindActor(const MPtr<FindActorMessage> &message) -> void;

    auto HandleFindActorWithCallback(const MPtr<FindActorWithCallbackMessage> &message) -> void;

    auto OnStop() -> void override;

    auto OnTerminate() -> void override;

    std::unordered_map<ActorId, ActorRef> actors_{};
};

} // namespace oxherdcpp