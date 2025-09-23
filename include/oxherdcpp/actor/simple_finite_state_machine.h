#pragma once

#include <optional>
#include <variant>

namespace oxherdcpp
{

template <typename Derived, typename... States> class GeneralFiniteStateMachine : public std::variant<States...>
{
  public:
    using State = std::variant<States...>;

    using State::operator=;

    constexpr GeneralFiniteStateMachine()
    {
        static_assert(std::is_base_of_v<GeneralFiniteStateMachine, Derived>, "Type Derived should be type Fsm");
    }

    template <typename Event> constexpr auto Dispatch(Event &&event) -> void
    {
        auto result = std::visit(
            [this, event = std::forward<Event>(event)](const auto &state) mutable -> std::optional<State> {
                auto &self = static_cast<Derived &>(*this);
                return self.On(state, std::forward<Event>(event));
            },
            static_cast<const State &>(*this));
        if (result)
        {
            *this = std::move(*result);
        }
    }

    template <typename U> [[nodiscard]] constexpr auto HasCurrentState() const noexcept -> bool
    {
        return std::get_if<U>(this) != nullptr;
    }
};
} // namespace oxherdcpp