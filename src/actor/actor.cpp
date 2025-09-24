#include <oxherdcpp/actor/actor.h>

#include <oxherdcpp/actor/actor_context.h>
#include <oxherdcpp/actor/events.h>
#include <oxherdcpp/logger/logger.h>

namespace oxherdcpp
{
Actor::Actor(const Executor &executor, std::string name, const ActorId actor_id)
    : strand_{boost::asio::make_strand(executor)}, name_{std::move(name)}, actor_id_{actor_id}
{
    InitializeMessageHandlers();
}

Actor::~Actor() = default;

auto Actor::Receive(MPtr<BaseMessage> message) -> void
{
    auto completion_token = [weak_self = this->weak_from_this(), message = std::move(message)]() mutable noexcept {
        const auto base = weak_self.lock();
        RETURN_IF(base == nullptr, void());

        base->ProcessMessage(message);
    };
    try
    {
        boost::asio::post(strand_, std::move(completion_token));
    }
    catch (const boost::system::system_error &e)
    {
        LOG_CRITICAL("Boost system error in post")
            .SetActorId(GetId())
            .SetActorName(GetName())
            .AddContext("exception message", e.what());
        std::terminate();
    }
    catch (std::exception &e)
    {
        LOG_CRITICAL("Standart exception in post")
            .SetActorId(GetId())
            .SetActorName(GetName())
            .AddContext("exception message", e.what());
        std::terminate();
    }
    catch (...)
    {
        LOG_CRITICAL("Unknown exception in post").SetActorId(GetId()).SetActorName(GetName());
        std::terminate();
    }
}

auto Actor::GetState() -> ActorState &
{
    return state_;
}

auto Actor::SetContext(Uptr<ActorContext> context) -> void
{
    context_ = std::move(context);
}
auto Actor::GetId() const -> ActorId
{
    return actor_id_;
}

auto Actor::GetName() const -> std::string
{
    return name_;
}

auto Actor::GetExecutor() const -> Executor
{
    return strand_.get_inner_executor();
}

auto Actor::GetContext() const -> ActorContext &
{
    if (!context_)
    {
        throw std::runtime_error("Context is not set");
    }
    return *context_;
}

auto Actor::GetMessageDispatcher() -> MessageDispatcher &
{
    return message_dispatcher_;
}

auto Actor::OnInitialize() -> void
{
}

auto Actor::OnStart() -> void
{
}

auto Actor::OnStarted() -> void
{
}

auto Actor::OnStop() -> void
{
}

auto Actor::OnStopped() -> void
{
}

auto Actor::OnPause() -> void
{
}

auto Actor::OnResume() -> void
{
}

auto Actor::OnTerminate() -> void
{
}

auto Actor::OnTerminated() -> void
{
}

auto Actor::InitializeMessageHandlers() -> void
{
    system_message_handlers_[GetTypeHash<GoStartActor>()] = [this] { HandleGoStart(); };
    system_message_handlers_[GetTypeHash<GoStopActor>()] = [this] { HandleGoStop(); };
    system_message_handlers_[GetTypeHash<GoPauseActor>()] = [this] { HandleGoPause(); };
    system_message_handlers_[GetTypeHash<GoResumeActor>()] = [this] { HandleGoResume(); };
    system_message_handlers_[GetTypeHash<GoTerminateActor>()] = [this] { HandleGoTerminate(); };
}

auto Actor::ProcessMessage(const MPtr<BaseMessage> &message) -> void
{
    const auto message_type = message->GetTypeId();

    if (const auto handler_it = system_message_handlers_.find(message_type);
        handler_it != system_message_handlers_.end())
    {
        handler_it->second();
    }
    else
    {
        HandleUserMessage(message);
    }
}

auto Actor::HandleGoStart() -> void
{
    if (state_.HasCurrentState<CreatedState>())
    {
        state_.Dispatch(InitializeEvent{});
        OnInitialize();
    }
    if (state_.HasCurrentState<InitializingState>() || state_.HasCurrentState<StoppedState>())
    {
        state_.Dispatch(StartEvent{});
        OnStart();
    }
    if (state_.HasCurrentState<StartingState>())
    {
        state_.Dispatch(StartedEvent{});
        OnStarted();
    }
}

auto Actor::HandleGoStop() -> void
{
    if (state_.HasCurrentState<RunningState>() || state_.HasCurrentState<PausedState>() ||
        state_.HasCurrentState<StartingState>())
    {
        state_.Dispatch(StopEvent{});
        OnStop();
    }
    if (state_.HasCurrentState<StoppingState>())
    {
        state_.Dispatch(StoppedEvent{});
        OnStopped();
    }
}

auto Actor::HandleGoPause() -> void
{
    if (state_.HasCurrentState<RunningState>())
    {
        state_.Dispatch(PauseEvent{});
        OnPause();
    }
}

auto Actor::HandleGoResume() -> void
{
    if (state_.HasCurrentState<PausedState>())
    {
        state_.Dispatch(ResumeEvent{});
        OnResume();
    }
}

auto Actor::HandleGoTerminate() -> void
{
    if (!state_.HasCurrentState<TerminatedState>())
    {
        state_.Dispatch(TerminateEvent{});
        OnTerminate();
    }
    if (state_.HasCurrentState<TerminatingState>())
    {
        state_.Dispatch(TerminatedEventReached{});
        OnTerminated();
    }
}

auto Actor::HandleUserMessage(const MPtr<BaseMessage> &message) -> void
{
    if (!state_.IsRunning())
    {
        return;
    }
    try
    {
        Behaviour(message);
    }
    catch (...)
    {
        state_.Dispatch(FailureEvent{});
        auto failure_event{MakeMessage<ActorFailureEvent>()};
        failure_event->actor_id = GetId();
        failure_event->actor_name = GetName();
        failure_event->cause = std::current_exception();
        failure_event->failed_message = message;

        if (const auto parent{GetContext().GetParent().lock()})
        {
            parent->Receive(std::move(failure_event));
        }
    }
}
} // namespace oxherdcpp