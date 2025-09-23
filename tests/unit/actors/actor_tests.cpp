#include <string>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

#include <oxherdcpp/actor/actor.h>
#include <oxherdcpp/actor/actor_context.h>
#include <oxherdcpp/actor/actor_system_facade.h>
#include <oxherdcpp/actor/events.h>

namespace testing
{

namespace ox = oxherdcpp;

struct TestMessage final : ox::Message<TestMessage>
{
};

class TestActor final : public ox::Actor
{
  public:
    using Actor::Actor;

    auto GetContextSelf() const -> Actor &
    {
        return GetContext().GetSelf();
    }
    auto GetContextParent() const -> ox::Wptr<Actor>
    {
        return GetContext().GetParent();
    }
    auto GetContextExecutor() const -> boost::asio::any_io_executor
    {
        return GetContext().GetExecutor();
    }

  protected:
    auto Behaviour(const ox::MPtr<ox::BaseMessage> &message) -> void override
    {
        (void)message;
    }
};

class DtorActor final : public ox::Actor
{
  public:
    using Actor::Actor;
    static inline std::atomic<int> dtor_called{0};
    ~DtorActor() override
    {
        dtor_called.fetch_add(1, std::memory_order_relaxed);
    }

  protected:
    void Behaviour(const ox::MPtr<ox::BaseMessage> &) override
    {
    }
};

class CounterActor final : public ox::Actor
{
  public:
    using Actor::Actor;
    std::atomic<int> calls{0};

  protected:
    void Behaviour(const ox::MPtr<ox::BaseMessage> &) override
    {
        calls.fetch_add(1, std::memory_order_relaxed);
    }
};

class ActorTests : public Test
{
  public:
    auto GetExecutor() -> boost::asio::any_io_executor
    {
        return io_context_.get_executor();
    }
    template <typename ActorType, typename... Args> auto CreateActor(Args &&...args) -> ox::Sptr<ActorType>
    {
        return ox::MakeSptr<ActorType>(io_context_.get_executor(), "Vanya", ox::ActorIDGenerator::Generate(),
                                       std::forward<Args>(args)...);
    }

    auto Start() -> void
    {
        io_context_.run();
    }
    auto Stop() -> void
    {
        io_context_.stop();
    }
    auto Restart() -> void
    {
        io_context_.restart();
    }
    auto Reset() -> void
    {
        io_context_.restart();
    }

  protected:
    auto SetUp() -> void override
    {
        ox::ReleaseMessagePoolMemory<TestMessage>();
    }

  private:
    boost::asio::io_context io_context_;
};

TEST_F(ActorTests, InitActorTest)
{
    const auto actor{CreateActor<TestActor>()};

    EXPECT_TRUE(actor);
}

TEST_F(ActorTests, ActorsHaveUniqueIdsOnCreation)
{
    constexpr int kCount{10};
    std::vector<ox::Sptr<TestActor>> actors;
    actors.reserve(kCount);

    std::unordered_set<ox::ActorId> ids;
    ids.reserve(kCount);

    for (int i{0}; i < kCount; ++i)
    {
        auto a{CreateActor<TestActor>()};
        ASSERT_TRUE(a);
        const auto id{a->GetId()};
        EXPECT_GT(id, 0);
        const auto inserted{ids.insert(id).second};
        EXPECT_TRUE(inserted) << "Duplicate actor id detected: " << id;
        actors.emplace_back(std::move(a));
    }

    EXPECT_EQ(static_cast<int>(ids.size()), kCount);
}

TEST_F(ActorTests, ActorInitialStateIsCreatedOnConstruction)
{
    const auto actor{CreateActor<TestActor>()};
    ASSERT_TRUE(actor);

    const auto &state{actor->GetState()};

    EXPECT_TRUE(state.HasCurrentState<ox::CreatedState>())
        << "Actor should be in CreatedState right after construction";
    EXPECT_FALSE(state.IsRunning());
    EXPECT_FALSE(state.IsPaused());
    EXPECT_FALSE(state.IsStopped());
    EXPECT_FALSE(state.IsTerminated());
}

TEST_F(ActorTests, ContextIsSetAndAccessible)
{
    class System : public ox::ActorSystemFacade, public std::enable_shared_from_this<System>
    {
      public:
        auto GetActorRegistry() -> ox::ActorRef override
        {
            return ox::ActorRef{1, weak_from_this()};
        }
        auto DispatchMessage(const ox::ActorId actor_id, const ox::MPtr<ox::BaseMessage> message) -> void override
        {
            (void)actor_id;
            (void)message;
        }
    };
    const auto actor{CreateActor<TestActor>()};
    ASSERT_TRUE(actor);

    EXPECT_THROW({ (void)actor->GetContextSelf(); }, std::runtime_error);

    const ox::Sptr<ox::Actor> parent{nullptr};

    auto system{ox::MakeSptr<System>()};

    auto ctx{ox::MakeUptr<ox::ActorContext>(GetExecutor(), parent, *actor, system)};
    actor->SetContext(std::move(ctx));

    EXPECT_EQ(&actor->GetContextSelf(), actor.get());
    EXPECT_EQ(actor->GetContextParent().lock(), nullptr);

    const auto ex{actor->GetContextExecutor()};
    ASSERT_TRUE(static_cast<bool>(ex.target_type().hash_code()));
}

TEST_F(ActorTests, TransitionsToRunningAndInvokesCallbacksInOrder)
{
    class TrackingActor final : public ox::Actor
    {
      public:
        using Actor::Actor;
        std::vector<std::string> calls;

      protected:
        void Behaviour(const ox::MPtr<ox::BaseMessage> &message) override
        {
            (void)message;
        }
        void OnInitialize() override
        {
            calls.emplace_back("OnInitialize");
        }
        void OnStart() override
        {
            calls.emplace_back("OnStart");
        }
        void OnStarted() override
        {
            calls.emplace_back("OnStarted");
        }
    };

    const auto actor{CreateActor<TrackingActor>()};

    ASSERT_TRUE(actor->GetState().HasCurrentState<ox::CreatedState>());

    actor->Receive(ox::MakeMessage<ox::GoStartActor>());

    Start();

    const std::vector<std::string> expected{"OnInitialize", "OnStart", "OnStarted"};
    EXPECT_EQ(actor->calls, expected);

    const auto &state{actor->GetState()};
    EXPECT_TRUE(state.HasCurrentState<ox::RunningState>()) << "Actor should be in RunningState after GoStartActor";
    EXPECT_TRUE(state.IsRunning());
    EXPECT_FALSE(state.IsPaused());
    EXPECT_FALSE(state.IsStopped());
    EXPECT_FALSE(state.IsTerminated());
}

TEST_F(ActorTests, TransitionsToStoppedAndInvokesCallbacksInOrderFromRunning)
{
    class TrackingActor final : public ox::Actor
    {
      public:
        using Actor::Actor;
        std::vector<std::string> calls;

      protected:
        void Behaviour(const ox::MPtr<ox::BaseMessage> &message) override
        {
            (void)message;
        }
        void OnInitialize() override
        {
            calls.emplace_back("OnInitialize");
        }
        void OnStart() override
        {
            calls.emplace_back("OnStart");
        }
        void OnStarted() override
        {
            calls.emplace_back("OnStarted");
        }
        void OnStop() override
        {
            calls.emplace_back("OnStop");
        }
        void OnStopped() override
        {
            calls.emplace_back("OnStopped");
        }
    };

    const auto actor{CreateActor<TrackingActor>()};

    actor->Receive(ox::MakeMessage<ox::GoStartActor>());
    Start();
    Restart();

    ASSERT_TRUE(actor->GetState().HasCurrentState<ox::RunningState>());

    actor->Receive(ox::MakeMessage<ox::GoStopActor>());
    Start();

    const std::vector<std::string> expected{"OnInitialize", "OnStart", "OnStarted", "OnStop", "OnStopped"};
    EXPECT_EQ(actor->calls, expected);

    const auto &state{actor->GetState()};
    EXPECT_TRUE(state.HasCurrentState<ox::StoppedState>()) << "Actor should be in StoppedState after GoStopActor";
    EXPECT_FALSE(state.IsRunning());
    EXPECT_FALSE(state.IsPaused());
    EXPECT_TRUE(state.IsStopped());
    EXPECT_FALSE(state.IsTerminated());
}

TEST_F(ActorTests, TransitionsToPausedFromRunning)
{
    class TrackingActor final : public ox::Actor
    {
      public:
        using Actor::Actor;
        std::vector<std::string> calls;

      protected:
        void Behaviour(const ox::MPtr<ox::BaseMessage> &message) override
        {
            (void)message;
        }
        void OnInitialize() override
        {
            calls.emplace_back("OnInitialize");
        }
        void OnStart() override
        {
            calls.emplace_back("OnStart");
        }
        void OnStarted() override
        {
            calls.emplace_back("OnStarted");
        }
        void OnPause() override
        {
            calls.emplace_back("OnPause");
        }
    };

    const auto actor{CreateActor<TrackingActor>()};

    actor->Receive(ox::MakeMessage<ox::GoStartActor>());
    Start();
    Restart();

    ASSERT_TRUE(actor->GetState().HasCurrentState<ox::RunningState>());

    actor->Receive(ox::MakeMessage<ox::GoPauseActor>());
    Start();

    const std::vector<std::string> expected{"OnInitialize", "OnStart", "OnStarted", "OnPause"};
    EXPECT_EQ(actor->calls, expected);

    const auto &state{actor->GetState()};
    EXPECT_TRUE(state.HasCurrentState<ox::PausedState>()) << "Actor should be in PausedState after GoPauseActor";
    EXPECT_FALSE(state.IsRunning());
    EXPECT_TRUE(state.IsPaused());
    EXPECT_FALSE(state.IsStopped());
    EXPECT_FALSE(state.IsTerminated());
}

TEST_F(ActorTests, TransitionsToRunningFromPaused)
{
    class TrackingActor final : public ox::Actor
    {
      public:
        using Actor::Actor;
        std::vector<std::string> calls;

      protected:
        void Behaviour(const ox::MPtr<ox::BaseMessage> &message) override
        {
            (void)message;
        }
        void OnInitialize() override
        {
            calls.emplace_back("OnInitialize");
        }
        void OnStart() override
        {
            calls.emplace_back("OnStart");
        }
        void OnStarted() override
        {
            calls.emplace_back("OnStarted");
        }
        void OnPause() override
        {
            calls.emplace_back("OnPause");
        }
        void OnResume() override
        {
            calls.emplace_back("OnResume");
        }
    };

    const auto actor{CreateActor<TrackingActor>()};

    actor->Receive(ox::MakeMessage<ox::GoStartActor>());
    Start();
    Restart();

    actor->Receive(ox::MakeMessage<ox::GoPauseActor>());
    Start();
    Restart();

    ASSERT_TRUE(actor->GetState().HasCurrentState<ox::PausedState>());

    actor->Receive(ox::MakeMessage<ox::GoResumeActor>());
    Start();

    const std::vector<std::string> expected{"OnInitialize", "OnStart", "OnStarted", "OnPause", "OnResume"};
    EXPECT_EQ(actor->calls, expected);

    const auto &state{actor->GetState()};
    EXPECT_TRUE(state.HasCurrentState<ox::RunningState>()) << "Actor should return to RunningState after GoResumeActor";
    EXPECT_TRUE(state.IsRunning());
    EXPECT_FALSE(state.IsPaused());
    EXPECT_FALSE(state.IsStopped());
    EXPECT_FALSE(state.IsTerminated());
}

TEST_F(ActorTests, TransitionsToTerminatedAndInvokesCallbacksInOrder)
{
    class TrackingActor final : public ox::Actor
    {
      public:
        using Actor::Actor;
        std::vector<std::string> calls;

      protected:
        void Behaviour(const ox::MPtr<ox::BaseMessage> &message) override
        {
            (void)message;
        }
        void OnInitialize() override
        {
            calls.emplace_back("OnInitialize");
        }
        void OnStart() override
        {
            calls.emplace_back("OnStart");
        }
        void OnStarted() override
        {
            calls.emplace_back("OnStarted");
        }
        void OnTerminate() override
        {
            calls.emplace_back("OnTerminate");
        }
        void OnTerminated() override
        {
            calls.emplace_back("OnTerminated");
        }
    };

    const auto actor{CreateActor<TrackingActor>()};

    actor->Receive(ox::MakeMessage<ox::GoStartActor>());
    Start();
    Restart();

    ASSERT_TRUE(actor->GetState().HasCurrentState<ox::RunningState>());

    actor->Receive(ox::MakeMessage<ox::GoTerminateActor>());
    Start();

    const std::vector<std::string> expected{"OnInitialize", "OnStart", "OnStarted", "OnTerminate", "OnTerminated"};
    EXPECT_EQ(actor->calls, expected);

    const auto &state{actor->GetState()};
    EXPECT_TRUE(state.HasCurrentState<ox::TerminatedState>())
        << "Actor should be in TerminatedState after GoTerminateActor";
    EXPECT_FALSE(state.IsRunning());
    EXPECT_FALSE(state.IsPaused());
    EXPECT_FALSE(state.IsStopped());
    EXPECT_TRUE(state.IsTerminated());
}

TEST_F(ActorTests, BehaviourIsCalledForUserMessagesInRunningState)
{
    struct UserMessage final : ox::Message<UserMessage>
    {
    };

    class BehaviourCountingActor final : public ox::Actor
    {
      public:
        using Actor::Actor;
        int behaviour_calls{0};

      protected:
        void Behaviour(const ox::MPtr<ox::BaseMessage> &message) override
        {
            (void)message;
            ++behaviour_calls;
        }
    };

    const auto actor{CreateActor<BehaviourCountingActor>()};

    actor->Receive(ox::MakeMessage<ox::GoStartActor>());
    Start();
    Restart();

    ASSERT_TRUE(actor->GetState().HasCurrentState<ox::RunningState>());

    actor->Receive(ox::MakeMessage<UserMessage>());
    Start();

    EXPECT_EQ(actor->behaviour_calls, 1) << "Behaviour must be called exactly once in Running state";
}

TEST_F(ActorTests, UserMessagesAreIgnoredInNonRunningStates)
{
    struct UserMessage final : ox::Message<UserMessage>
    {
    };

    class BehaviourCountingActor final : public ox::Actor
    {
      public:
        using Actor::Actor;
        int behaviour_calls{0};

      protected:
        void Behaviour(const ox::MPtr<ox::BaseMessage> &message) override
        {
            (void)message;
            ++behaviour_calls;
        }
    };
    {
        const auto actor{CreateActor<BehaviourCountingActor>()};
        ASSERT_TRUE(actor->GetState().HasCurrentState<ox::CreatedState>());
        actor->Receive(ox::MakeMessage<UserMessage>());
        Start();
        EXPECT_EQ(actor->behaviour_calls, 0) << "Behaviour must be ignored in Created state";
        Restart();
    }
    {
        const auto actor{CreateActor<BehaviourCountingActor>()};
        actor->Receive(ox::MakeMessage<ox::GoStartActor>());
        Start();
        Restart();
        ASSERT_TRUE(actor->GetState().HasCurrentState<ox::RunningState>());
        actor->Receive(ox::MakeMessage<ox::GoPauseActor>());
        Start();
        Restart();
        ASSERT_TRUE(actor->GetState().HasCurrentState<ox::PausedState>());

        actor->Receive(ox::MakeMessage<UserMessage>());
        Start();
        EXPECT_EQ(actor->behaviour_calls, 0) << "Behaviour must be ignored in Paused state";
        Restart();
    }
    {
        const auto actor{CreateActor<BehaviourCountingActor>()};
        actor->Receive(ox::MakeMessage<ox::GoStartActor>());
        Start();
        Restart();
        ASSERT_TRUE(actor->GetState().HasCurrentState<ox::RunningState>());
        actor->Receive(ox::MakeMessage<ox::GoStopActor>());
        Start();
        ASSERT_TRUE(actor->GetState().HasCurrentState<ox::StoppedState>());

        actor->Receive(ox::MakeMessage<UserMessage>());
        Start();
        EXPECT_EQ(actor->behaviour_calls, 0) << "Behaviour must be ignored in Stopped state";
        Restart();
    }
    {
        const auto actor{CreateActor<BehaviourCountingActor>()};
        actor->Receive(ox::MakeMessage<ox::GoStartActor>());
        Start();
        Restart();
        actor->Receive(ox::MakeMessage<ox::GoTerminateActor>());
        Start();
        ASSERT_TRUE(actor->GetState().HasCurrentState<ox::TerminatedState>());

        actor->Receive(ox::MakeMessage<UserMessage>());
        Start();
        EXPECT_EQ(actor->behaviour_calls, 0) << "Behaviour must be ignored in Terminated state";
        Restart();
    }
}

TEST_F(ActorTests, StrandProcessesMessagesSequentiallyInMultithreadedScenario)
{
    struct SeqMessage final : ox::Message<SeqMessage>
    {
        explicit SeqMessage(const uint32_t n) : value(n)
        {
        }
        uint32_t value;
    };

    class SeqActor final : public ox::Actor
    {
      public:
        using Actor::Actor;

        std::vector<uint32_t> received;
        std::atomic<int> in_behaviour{0};

      protected:
        void Behaviour(const ox::MPtr<ox::BaseMessage> &message) override
        {
            EXPECT_EQ(in_behaviour.fetch_add(1, std::memory_order_acq_rel), 0);

            if (message->IsA<SeqMessage>())
            {
                const auto *m{static_cast<SeqMessage *>(message.get())};
                received.push_back(m->value);
            }

            EXPECT_EQ(in_behaviour.fetch_sub(1, std::memory_order_acq_rel), 1);
        }
    };

    const auto actor{CreateActor<SeqActor>()};
    actor->Receive(ox::MakeMessage<ox::GoStartActor>());
    Start();
    Restart();

    ASSERT_TRUE(actor->GetState().HasCurrentState<ox::RunningState>());

    constexpr uint32_t kTotal{5000};
    const unsigned kThreads{std::max(2u, std::thread::hardware_concurrency())};

    std::atomic<uint32_t> next_index{0};
    std::atomic<uint32_t> turn{0};

    auto producer = [&] {
        while (true)
        {
            uint32_t idx = next_index.fetch_add(1, std::memory_order_relaxed);
            if (idx >= kTotal)
            {
                break;
            }

            while (turn.load(std::memory_order_acquire) != idx)
            {
            }
            actor->Receive(ox::MakeMessage<SeqMessage>(idx));

            turn.fetch_add(1, std::memory_order_release);
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (unsigned i{0}; i < kThreads; ++i)
    {
        threads.emplace_back(producer);
    }
    for (auto &t : threads)
    {
        t.join();
    }

    Start();
    Restart();

    ASSERT_EQ(actor->received.size(), static_cast<size_t>(kTotal));
    for (uint32_t i{0}; i < kTotal; ++i)
    {
        EXPECT_EQ(actor->received[i], i) << "Strand must preserve FIFO order of posted handlers";
    }
}

TEST_F(ActorTests, DestructorIsInvokedOnce)
{
    DtorActor::dtor_called = 0;
    {
        const auto actor{CreateActor<DtorActor>()};
        actor->Receive(ox::MakeMessage<ox::GoStartActor>());
        Start();
        Restart();
    }
    EXPECT_EQ(DtorActor::dtor_called.load(), 1);
}

TEST_F(ActorTests, InflightUserMessagesAreNotExecutedAfterReset)
{
    auto actor{CreateActor<CounterActor>()};
    actor->Receive(ox::MakeMessage<ox::GoStartActor>());
    Start();
    Restart();

    for (int i{0}; i < 1000; ++i)
    {
        actor->Receive(ox::MakeMessage<ox::GoStartActor>());
    }
    actor->Receive(ox::MakeMessage<ox::GoStartActor>());

    actor.reset();

    Start();
    Restart();

    SUCCEED();
}

TEST_F(ActorTests, ConcurrentSystemTransitionsAreSerializedAndStateRemainsValid)
{
    class TrackingActor final : public ox::Actor
    {
      public:
        using Actor::Actor;

        std::atomic<int> in_cb{0};
        std::mutex mtx;
        std::vector<std::string> calls;

      protected:
        void Behaviour(const ox::MPtr<ox::BaseMessage> &) override
        {
        }

        void OnInitialize() override
        {
            Guard("OnInitialize");
        }
        void OnStart() override
        {
            Guard("OnStart");
        }
        void OnStarted() override
        {
            Guard("OnStarted");
        }
        void OnStop() override
        {
            Guard("OnStop");
        }
        void OnStopped() override
        {
            Guard("OnStopped");
        }
        void OnPause() override
        {
            Guard("OnPause");
        }
        void OnResume() override
        {
            Guard("OnResume");
        }
        void OnTerminate() override
        {
            Guard("OnTerminate");
        }
        void OnTerminated() override
        {
            Guard("OnTerminated");
        }

        void Guard(const char *name)
        {
            EXPECT_EQ(in_cb.fetch_add(1, std::memory_order_acq_rel), 0) << "Callbacks must not run concurrently";
            {
                std::lock_guard lk(mtx);
                calls.emplace_back(name);
            }
            EXPECT_EQ(in_cb.fetch_sub(1, std::memory_order_acq_rel), 1);
        }
    };

    const auto actor{CreateActor<TrackingActor>()};

    constexpr int kThreads{8};
    constexpr int kPerThread{500};

    std::atomic start{false};
    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    auto send = [&](const int tid) {
        while (!start.load(std::memory_order_acquire))
        { /* spin */
        }
        for (int i = 0; i < kPerThread; ++i)
        {
            switch ((tid + i) % 5)
            {
            case 0:
                actor->Receive(ox::MakeMessage<ox::GoStartActor>());
                break;
            case 1:
                actor->Receive(ox::MakeMessage<ox::GoPauseActor>());
                break;
            case 2:
                actor->Receive(ox::MakeMessage<ox::GoResumeActor>());
                break;
            case 3:
                actor->Receive(ox::MakeMessage<ox::GoStopActor>());
                break;
            case 4:
                actor->Receive(ox::MakeMessage<ox::GoTerminateActor>());
                break;
            default:;
            }
        }
    };

    for (int t{0}; t < kThreads; ++t)
        threads.emplace_back(send, t);
    start.store(true, std::memory_order_release);
    for (auto &th : threads)
        th.join();

    Start();
    Restart();

    const auto &st{actor->GetState()};
    const bool valid_final{st.HasCurrentState<ox::RunningState>() || st.HasCurrentState<ox::PausedState>() ||
                           st.HasCurrentState<ox::StoppedState>() || st.HasCurrentState<ox::TerminatedState>()};
    EXPECT_TRUE(valid_final) << "Final state must be one of stable states";

    EXPECT_EQ(actor->in_cb.load(), 0);
}

} // namespace testing