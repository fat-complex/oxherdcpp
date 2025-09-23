#pragma once

#include <oxherdcpp/actor/message/message.h>

namespace oxherdcpp
{

class MessageDispatcher
{
    template <typename MessageType> using Handler = std::function<void(const MPtr<MessageType> &message)>;
    using HandlerMap = std::unordered_map<MessageTypeID, Handler<BaseMessage>>;

  public:
    MessageDispatcher() = default;

    template <typename MessageType> auto RegisterHandler(Handler<MessageType> handler) -> MessageDispatcher &
    {
        auto type_id{GetTypeHash<MessageType>()};
        handlers_[type_id] = [handler = std::move(handler)](const MPtr<BaseMessage> &message) {
            handler(Cast<MessageType>(message));
        };
        return *this;
    }

    auto Dispatch(const MPtr<BaseMessage> &message) -> void;

  private:
    HandlerMap handlers_{};
};

} // namespace oxherdcpp