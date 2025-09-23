#include <oxherdcpp/actor/supervision/supervision_strategy.h>

#include <ranges>

#include <oxherdcpp/actor/events.h>

namespace oxherdcpp
{

Directive OneForOneStrategy::Decide(const MPtr<ActorFailureEvent> &failure)
{
    for (const auto &handler : exception_handlers_ | std::views::values)
    {
        return handler->Handle(failure->cause);
    }
    return default_directive_;
}

auto OneForOneStrategy::SetDefaultDirective(const Directive default_directive) -> OneForOneStrategy &
{
    default_directive_ = default_directive;
    return *this;
}
} // namespace oxherdcpp