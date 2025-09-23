#include <oxherdcpp/actor/actor_context.h>

#include <oxherdcpp/actor/actor_registry.h>
#include <oxherdcpp/actor/actor_system.h>
#include <oxherdcpp/actor/events.h>

namespace oxherdcpp
{

ActorContext::ActorContext(boost::asio::any_io_executor executor, const Sptr<Actor> &parent, Actor &self,
                           Wptr<ActorSystemFacade> facade)
    : executor_{std::move(executor)}, parent_{parent}, self_{self}, system_facade_{std::move(facade)}
{
}

auto ActorContext::GetSelf() const -> Actor &
{
    return self_;
}

auto ActorContext::GetParent() const -> Wptr<Actor>
{
    return parent_;
}

auto ActorContext::GetExecutor() -> boost::asio::any_io_executor
{
    return executor_;
}

auto ActorContext::HandleChildFailure(const MPtr<ActorFailureEvent> &failure_event) -> void
{
    auto escalate_to_parent{[this, failure_event] {
        if (const auto parent = parent_.lock())
        {
            auto escalation = MakeMessage<ActorFailureEvent>();
            escalation->actor_id = self_.GetId();
            escalation->actor_name = self_.GetName();
            escalation->cause = failure_event->cause;
            escalation->failed_message = failure_event;
            parent->Receive(std::move(escalation));
        }
    }};
    const auto child_it{children_.find(failure_event->actor_id)};
    if (child_it == children_.end() || child_it->second.strategy == nullptr)
    {
        escalate_to_parent();
        return;
    }
    const auto &info{child_it->second};
    const auto &actor{info.actor};

    switch (const auto &strategy{info.strategy}; strategy->Decide(failure_event))
    {
    case Directive::Resume:
        actor->Receive(MakeMessage<GoResumeActor>());
        break;

    case Directive::Restart: {
        RestartChildActor(child_it->second);
        break;
    }

    case Directive::Stop: {
        actor->Receive(MakeMessage<GoStopActor>());
        break;
    }

    case Directive::Escalate:
        escalate_to_parent();
        break;
    }
}

auto ActorContext::SpawnChildImpl(std::function<Sptr<Actor>()> factory, Uptr<SupervisionStrategy> strategy) -> ActorRef
{
    auto child{factory()};
    auto ref{ActorRef{child, system_facade_}};

    children_[child->GetId()] =
        ChildInfo{.actor = std::move(child), .strategy = std::move(strategy), .factory = std::move(factory)};

    return ref;
}

auto ActorContext::RestartChildActor(ChildInfo &child_info) -> void
{
    auto &[actor, strategy, factory]{child_info};

    actor->Receive(MakeMessage<GoTerminateActor>());

    auto old_strategy{std::move(strategy)};
    auto old_factory{std::move(factory)};

    children_.erase(actor->GetId());

    ChildInfo info{};
    info.actor = old_factory();
    info.strategy = std::move(old_strategy);
    info.factory = std::move(old_factory);
    children_[info.actor->GetId()] = std::move(info);

    if (const auto sys{system_facade_.lock()})
    {
        sys->GetActorRegistry().Tell(
            MakeMessage<RegisterActorMessage>(info.actor->GetId(), ActorRef{info.actor, system_facade_}));
    }
}

} // namespace oxherdcpp