#include <gtest/gtest.h>

#include <oxherdcpp/actor/simple_finite_state_machine.h>

namespace testing
{

namespace ox = oxherdcpp;

//  ----------------- *
//  |   start         |
//  |     |           |
//  *---> A --> B --> C
//      __|           ^
//      |             |
//      *-> D --------*

// States
struct A
{
};
struct B
{
};
struct C
{
};
struct D
{
};

// Events
struct ABEvent
{
};
struct ADEvent
{
};
struct BCEvent
{
};
struct CAEvent
{
};
struct DCEvent
{
};

class StateMachine : public ox::GeneralFiniteStateMachine<StateMachine, A, B, C, D>
{
  public:
    template <typename S, typename E> static auto On(const S &, const E &)
    {
        return std::nullopt;
    }

    static State On(const A &, const ABEvent &)
    {
        return B{};
    }

    static State On(const A &, const ADEvent &)
    {
        return D{};
    }

    static State On(const B &, const BCEvent &)
    {
        return C{};
    }

    static State On(const C &, const CAEvent &)
    {
        return A{};
    }

    static State On(const D &, const DCEvent &)
    {
        return C{};
    }
};

TEST(FiniteStateMachine, checkInstance)
{
    constexpr StateMachine stateMachine;
    EXPECT_TRUE(stateMachine.HasCurrentState<A>());
}

TEST(FiniteStateMachine, checkABTransition)
{
    StateMachine stateMachine;
    stateMachine.Dispatch(ABEvent{});
    EXPECT_TRUE(stateMachine.HasCurrentState<B>());

    stateMachine.Dispatch(ADEvent{});
    EXPECT_FALSE(stateMachine.HasCurrentState<D>());
}

TEST(FiniteStateMachine, checkADTransition)
{
    StateMachine stateMachine;
    stateMachine.Dispatch(ADEvent{});
    EXPECT_TRUE(stateMachine.HasCurrentState<D>());

    stateMachine.Dispatch(CAEvent{});
    EXPECT_FALSE(stateMachine.HasCurrentState<A>());
}

TEST(FiniteStateMachine, checkBCTransition)
{
    StateMachine stateMachine;
    stateMachine.Dispatch(ABEvent{});
    EXPECT_TRUE(stateMachine.HasCurrentState<B>());
    stateMachine.Dispatch(BCEvent{});
    EXPECT_TRUE(stateMachine.HasCurrentState<C>());

    stateMachine.Dispatch(ADEvent{});
    EXPECT_FALSE(stateMachine.HasCurrentState<D>());
}

TEST(FiniteStateMachine, checkCATransition)
{
    StateMachine stateMachine;
    stateMachine.Dispatch(ABEvent{});
    stateMachine.Dispatch(BCEvent{});
    stateMachine.Dispatch(CAEvent{});
    EXPECT_TRUE(stateMachine.HasCurrentState<A>());

    stateMachine.Dispatch(DCEvent{});
    EXPECT_FALSE(stateMachine.HasCurrentState<C>());
}

TEST(FiniteStateMachine, checkDCTransition)
{
    StateMachine stateMachine;
    stateMachine.Dispatch(ADEvent{});
    stateMachine.Dispatch(DCEvent{});
    EXPECT_TRUE(stateMachine.HasCurrentState<C>());

    stateMachine.Dispatch(ADEvent{});
    EXPECT_FALSE(stateMachine.HasCurrentState<D>());
}

TEST(FiniteStateMachine, checkACTransitionLoop)
{
    StateMachine stateMachine;
    stateMachine.Dispatch(ADEvent{});
    stateMachine.Dispatch(DCEvent{});
    stateMachine.Dispatch(CAEvent{});
    EXPECT_TRUE(stateMachine.HasCurrentState<A>());

    stateMachine.Dispatch(ADEvent{});
    EXPECT_TRUE(stateMachine.HasCurrentState<D>());
}

} // namespace testing