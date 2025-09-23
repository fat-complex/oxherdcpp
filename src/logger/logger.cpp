#include <oxherdcpp/logger/logger.h>

namespace oxherdcpp
{
auto GlobalLoggingSystem::Initialize(const std::shared_ptr<Logger> &logger) -> void
{
    logger_ = logger;
}

auto GlobalLoggingSystem::Shutdown() -> void
{
    if (logger_)
    {
        logger_->Flush();
        logger_.reset();
    }
}
auto GlobalLoggingSystem::GetLogger() -> Logger &
{
    if (!logger_)
    {
        throw std::runtime_error("Global logging system not initialized");
    }
    return *logger_;
}

auto GlobalLoggingSystem::IsInitialized() -> bool
{
    return logger_ != nullptr;
}
} // namespace oxherdcpp