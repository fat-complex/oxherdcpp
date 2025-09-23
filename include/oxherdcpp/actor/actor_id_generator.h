#pragma once

#include <oxherdcpp/common/uuid.h>

namespace oxherdcpp
{

using ActorId = std::size_t;

using ActorIDGenerator = IDGenerator<struct ActorTag, ActorId>;

} // namespace oxherdcpp
