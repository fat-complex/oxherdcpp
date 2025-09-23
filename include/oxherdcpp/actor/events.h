#pragma once

#include <exception>

#include <oxherdcpp/actor/actor_id_generator.h>
#include <oxherdcpp/actor/message/message.h>

namespace oxherdcpp
{
struct ActorFailureEvent final : Message<ActorFailureEvent>
{
    ActorId actor_id;
    std::string actor_name;
    std::exception_ptr cause;
    BaseMessagePtr failed_message;
};

struct GoStartActor final : Message<GoStartActor>
{
};

struct GoStopActor final : Message<GoStopActor>
{
};

struct GoPauseActor final : Message<GoPauseActor>
{
};

struct GoResumeActor final : Message<GoResumeActor>
{
};

struct GoTerminateActor final : Message<GoTerminateActor>
{
};
} // namespace oxherdcpp