#pragma once

#include <boost/asio.hpp>

#include <oxherdcpp/actor/actor_id_generator.h>
#include <oxherdcpp/actor/actor_state.h>
#include <oxherdcpp/actor/message/message_dispatcher.h>
#include <oxherdcpp/common/helper_macros.h>
#include <oxherdcpp/common/memory.h>

namespace oxherdcpp
{

class ActorContext;

using Executor = boost::asio::any_io_executor;

class Actor : public std::enable_shared_from_this<Actor>
{
    DISABLE_COPY_AND_MOVE(Actor)
  public:
    explicit Actor(const Executor &executor, std::string name, ActorId actor_id);

    virtual ~Actor();

    auto Receive(MPtr<BaseMessage> message) -> void;

    auto GetState() -> ActorState &;

    auto SetContext(Uptr<ActorContext> context) -> void;

    [[nodiscard]] auto GetId() const -> ActorId;

    auto GetName() const -> std::string;

  protected:
    [[nodiscard]] auto GetExecutor() const -> Executor;

    [[nodiscard]] auto GetContext() const -> ActorContext &;

    [[nodiscard]] auto GetMessageDispatcher() -> MessageDispatcher &;

    virtual auto OnInitialize() -> void;
    virtual auto OnStart() -> void;
    virtual auto OnStarted() -> void;
    virtual auto OnStop() -> void;
    virtual auto OnStopped() -> void;
    virtual auto OnPause() -> void;
    virtual auto OnResume() -> void;
    virtual auto OnTerminate() -> void;
    virtual auto OnTerminated() -> void;

  private:
    virtual auto Behaviour(const MPtr<BaseMessage> &message) -> void = 0;

    using MessageHandler = std::function<void()>;
    using MessageHandlerMap = std::unordered_map<MessageTypeID, MessageHandler>;

    auto InitializeMessageHandlers() -> void;

    auto ProcessMessage(const MPtr<BaseMessage> &message) -> void;

    auto HandleGoStart() -> void;

    auto HandleGoStop() -> void;

    auto HandleGoPause() -> void;

    auto HandleGoResume() -> void;

    auto HandleGoTerminate() -> void;

    auto HandleUserMessage(const MPtr<BaseMessage> &message) -> void;

    boost::asio::strand<Executor> strand_{};
    std::string name_;
    ActorId actor_id_;
    Uptr<ActorContext> context_;

    ActorState state_{};
    MessageHandlerMap system_message_handlers_;
    MessageDispatcher message_dispatcher_{};
};

} // namespace oxherdcpp