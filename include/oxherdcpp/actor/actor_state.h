#pragma once

#include <oxherdcpp/actor/simple_finite_state_machine.h>

namespace oxherdcpp
{

struct CreatedState
{
};
struct InitializingState
{
};
struct StartingState
{
};
struct RunningState
{
};
struct PausedState
{
};
struct StoppingState
{
};
struct StoppedState
{
};
struct TerminatingState
{
};
struct TerminatedState
{
};

struct InitializeEvent
{
};
struct StartEvent
{
};
struct StartedEvent
{
};
struct StopEvent
{
};
struct StoppedEvent
{
};
struct TerminateEvent
{
};
struct TerminatedEventReached
{
};
struct PauseEvent
{
};
struct ResumeEvent
{
};
struct FailureEvent
{
};

class ActorState final
    : public GeneralFiniteStateMachine<ActorState, CreatedState, InitializingState, StartingState, RunningState,
                                       StoppingState, StoppedState, TerminatingState, TerminatedState, PausedState>
{
  public:
    constexpr ActorState() = default;

    static auto On(const CreatedState &, const InitializeEvent &) -> State
    {
        return InitializingState{};
    }

    static auto On(const InitializingState &, const StartEvent &) -> State
    {
        return StartingState{};
    }

    static auto On(const StartingState &, const StartedEvent &) -> State
    {
        return RunningState{};
    }

    static auto On(const RunningState &, const StopEvent &) -> State
    {
        return StoppingState{};
    }

    static auto On(const StoppingState &, const StoppedEvent &) -> State
    {
        return StoppedState{};
    }

    static auto On(const StoppedState &, const StartEvent &) -> State
    {
        return StartingState{};
    }

    template <typename S> static auto On(const S &, const TerminateEvent &) -> State
    {
        return TerminatingState{};
    }

    static auto On(const TerminatingState &, const TerminatedEventReached &) -> State
    {
        return TerminatedState{};
    }

    static auto On(const RunningState &, const PauseEvent &) -> State
    {
        return PausedState{};
    }

    static auto On(const PausedState &, const ResumeEvent &) -> State
    {
        return RunningState{};
    }

    static auto On(const PausedState &, const StopEvent &) -> State
    {
        return StoppingState{};
    }

    template <typename S> static auto On(const S &, const FailureEvent &) -> State
    {
        return StoppingState{};
    }

    template <typename S, typename E> static auto On(const S &, const E &) -> std::optional<State>
    {
        // LOG_WARNING ?
        return std::nullopt;
    }

    [[nodiscard]] bool IsStopped() const
    {
        return HasCurrentState<StoppedState>();
    }
    [[nodiscard]] bool IsRunning() const
    {
        return HasCurrentState<RunningState>();
    }
    [[nodiscard]] bool IsPaused() const
    {
        return HasCurrentState<PausedState>();
    }
    [[nodiscard]] bool IsTerminated() const
    {
        return HasCurrentState<TerminatedState>();
    }
};

} // namespace oxherdcpp