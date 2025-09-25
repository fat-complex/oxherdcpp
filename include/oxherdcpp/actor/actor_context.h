#pragma once

#include <boost/asio.hpp>

#include <oxherdcpp/actor/actor.h>
#include <oxherdcpp/actor/actor_ref.h>
#include <oxherdcpp/actor/supervision/supervision_strategy.h>
#include <oxherdcpp/common/helper_macros.h>

namespace oxherdcpp
{

class ActorSystemFacade;
class SupervisionStrategy;

class ActorContext
{
    struct ChildInfo
    {
        Sptr<Actor> actor{};
        Uptr<SupervisionStrategy> strategy{};
        std::function<Sptr<Actor>()> factory{};
    };
    DISABLE_COPY_AND_MOVE(ActorContext)
  public:
    ActorContext(boost::asio::any_io_executor executor, const Sptr<Actor> &parent, Actor &self,
                 Wptr<ActorSystemFacade> facade);

    [[nodiscard]] auto GetSelf() const -> Actor &;
    [[nodiscard]] auto GetParent() const -> Wptr<Actor>;
    auto GetExecutor() -> boost::asio::any_io_executor;

    template <typename ActorType, typename... Args>
    auto SpawnChild(std::string name, Uptr<SupervisionStrategy> supervision_strategy, Args &&...args) -> ActorRef
    {
        auto factory{[this, name = std::move(name), actor_id = ActorIDGenerator::Generate(),
                      ... args = std::forward<Args>(args)]() mutable {
            return CreateActor<ActorType>(name, actor_id, std::forward<decltype(args)>(args)...);
        }};
        auto shared_factory{MakeSptr<decltype(factory)>(std::move(factory))};

        std::function<Sptr<Actor>()> copyable_factory{[shared_factory]() mutable { return (*shared_factory)(); }};
        return SpawnChildImpl(std::move(copyable_factory), std::move(supervision_strategy));
    }

    auto HandleChildFailure(const MPtr<ActorFailureEvent> &failure_event) -> void;

  private:
    template <typename ActorType, typename... Args>
    auto CreateActor(std::string name, ActorId actor_id, Args &&...args) -> Sptr<Actor>
    {
        auto actor{MakeSptr<ActorType>(GetExecutor(), std::move(name), actor_id, std::forward<Args>(args)...)};
        auto context{MakeUptr<ActorContext>(GetExecutor(), self_.shared_from_this(), *actor, system_facade_)};
        actor->SetContext(std::move(context));

        return actor;
    }

    auto SpawnChildImpl(std::function<Sptr<Actor>()> factory, Uptr<SupervisionStrategy> strategy) -> ActorRef;

    auto RestartChildActor(ChildInfo &child_info) -> void;

    boost::asio::any_io_executor executor_;
    Wptr<Actor> parent_;
    Actor &self_;
    std::unordered_map<ActorId, ChildInfo> children_;

    Wptr<ActorSystemFacade> system_facade_;
};

} // namespace oxherdcpp