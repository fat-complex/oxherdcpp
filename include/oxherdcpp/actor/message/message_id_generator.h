#pragma once

#include <oxherdcpp/common/uuid.h>

namespace oxherdcpp
{

using MessageTypeID = std::size_t;

using MessageIDGenerator = IDGenerator<struct MessageTag, MessageTypeID>;

} // namespace oxherdcpp
