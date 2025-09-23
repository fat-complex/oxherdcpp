#include <oxherdcpp/actor/actor_registry.h>

#include <oxherdcpp/logger/logger.h>

namespace oxherdcpp
{

ActorRegistry::ActorRegistry(const Executor &executor, const std::string &name, const ActorId actor_id)
    : Actor{executor, name, actor_id}
{
    GetMessageDispatcher()
        .RegisterHandler<RegisterActorMessage>([this](const auto &msg) { HandleRegisterActor(msg); })
        .RegisterHandler<UnregisterActorMessage>([this](const auto &msg) { HandleUnregisterActor(msg); })
        .RegisterHandler<FindActorMessage>([this](const auto &msg) { HandleFindActor(msg); })
        .RegisterHandler<FindActorWithCallbackMessage>([this](const auto &msg) { HandleFindActorWithCallback(msg); });
}

auto ActorRegistry::Behaviour(const MPtr<BaseMessage> &message) -> void
{
    GetMessageDispatcher().Dispatch(message);
}

auto ActorRegistry::HandleRegisterActor(const MPtr<RegisterActorMessage> &message) -> void
{
    actors_.insert_or_assign(message->actor_id, message->actor_ref);
    if (actors_.contains(message->actor_id))
    {
        LOG_INFO("Registered actor ", message->actor_id);
    }
}

auto ActorRegistry::HandleUnregisterActor(const MPtr<UnregisterActorMessage> &message) -> void
{
    actors_.erase(message->actor_id);
}

auto ActorRegistry::HandleFindActor(const MPtr<FindActorMessage> &message) -> void
{
    if (const auto found_it{actors_.find(message->actor_id)}; found_it != actors_.end())
    {
        message->reply_to.Tell(MakeMessage<ActorFoundResponseMessage>(found_it->second));
    }
    else
    {
        message->reply_to.Tell(MakeMessage<ActorNotFoundResponseMessage>(message->actor_id));
    }
}

auto ActorRegistry::HandleFindActorWithCallback(const MPtr<FindActorWithCallbackMessage> &message) -> void
{
    if (const auto found_it{actors_.find(message->actor_id)}; found_it != actors_.end())
    {
        message->callback(found_it->second);
    }
}

void ActorRegistry::OnStop()
{
    actors_.clear();
}

void ActorRegistry::OnTerminate()
{
    actors_.clear();
}
} // namespace oxherdcpp