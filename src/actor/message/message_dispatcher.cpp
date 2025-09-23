#include <oxherdcpp/actor/message/message_dispatcher.h>

namespace oxherdcpp
{

auto MessageDispatcher::Dispatch(const MPtr<BaseMessage> &message) -> void
{
    if (const auto found_it{handlers_.find(message->GetTypeId())}; found_it != handlers_.end())
    {
        found_it->second(message);
    }
}
} // namespace oxherdcpp