#pragma once

#include <string>
#include <thread>

#include <boost/asio.hpp>

#include <oxherdcpp/actor/actor_context.h>
#include <oxherdcpp/actor/actor_system_facade.h>
#include <oxherdcpp/common/helper_macros.h>
#include <oxherdcpp/common/memory.h>

namespace oxherdcpp
{

class Logger;

class ActorSystem final : public ActorSystemFacade, public std::enable_shared_from_this<ActorSystem>
{
    DISABLE_COPY_AND_MOVE(ActorSystem)
  public:
    explicit ActorSystem(std::string name, std::size_t thread_count = std::thread::hardware_concurrency());

    ~ActorSystem() override;

    auto GetActorRegistry() -> ActorRef override;

    auto DispatchMessage(ActorId actor_id, MPtr<BaseMessage> message) -> void override;

    auto GetExecutor() -> boost::asio::any_io_executor;

    auto Stop() -> void;

    template <typename ActorType, typename... Args>
    auto CreateActor(const std::string &name, Args &&...args) -> Sptr<ActorType>
    {
        auto actor{MakeSptr<ActorType>(GetExecutor(), name, ActorIDGenerator::Generate(), std::forward<Args>(args)...)};
        auto context{MakeUptr<ActorContext>(GetExecutor(), nullptr, *actor, weak_from_this())};
        actor->SetContext(std::move(context));

        return actor;
    }

  private:
    auto InitRuntime() -> void;

    auto InitServices() -> void;

    using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
    std::string name_;
    std::atomic<bool> is_running_{false};

    // Execution context and thread pool
    boost::asio::io_context io_context_;
    std::optional<WorkGuard> work_guard_;
    std::size_t thread_count_;
    std::vector<std::jthread> thread_pool_;

    Sptr<Actor> actor_registry_;
};

} // namespace oxherdcpp