#pragma once

#include <sstream>
#include <string>
#include <unordered_map>

#include <oxherdcpp/common/helper_macros.h>
#include <oxherdcpp/common/memory.h>
#include <oxherdcpp/actor/actor_id_generator.h>
#include <oxherdcpp/actor/message/message.h>

namespace oxherdcpp
{

using LogContext = std::unordered_map<std::string, std::string>;

enum class LogLevel
{
    TRACE = 0,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

struct SourceLocation
{
    std::string file;
    int line{};
    std::string function;
};

struct TraceContext
{
    std::string trace_id;
    std::string span_id;
};

struct LogMessage final : Message<LogMessage>
{
    LogLevel level{};
    std::string message;
    LogContext context;
    ActorId actor_id{};
    std::string actor_name;
    std::chrono::system_clock::time_point timestamp;
    SourceLocation location;
    TraceContext trace_context;
};

class Logger
{
  public:
    virtual ~Logger() = default;

    virtual auto Log(const MPtr<LogMessage> &log_message) -> void = 0;

    virtual auto Flush() -> void = 0;

    virtual auto SetLevel(LogLevel level) -> void = 0;

    [[nodiscard]] virtual auto GetLevel() const -> LogLevel = 0;
};

class GlobalLoggingSystem final
{
  public:
    static auto Initialize(const Sptr<Logger> &logger) -> void;
    static auto Shutdown() -> void;

    static auto GetLogger() -> Logger &;

    static auto IsInitialized() -> bool;

  private:
    inline static std::shared_ptr<Logger> logger_;
};

template <typename... Args> auto MakeMessageFormat(Args &&...args) -> std::string
{
    std::stringstream ss;
    (ss << ... << args);
    return ss.str();
}

class MessageBuilder
{
  public:
    DISABLE_COPY_AND_MOVE(MessageBuilder)

    MessageBuilder(const LogLevel level, SourceLocation location, std::string message)
        : message_{MakeMessage<LogMessage>()}
    {
        message_->level = level;
        message_->location = std::move(location);
        message_->message = std::move(message);
        message_->timestamp = std::chrono::system_clock::now();
    }

    ~MessageBuilder()
    {
        if (GlobalLoggingSystem::IsInitialized() && message_->level >= GlobalLoggingSystem::GetLogger().GetLevel())
        {
            GlobalLoggingSystem::GetLogger().Log(message_);
        }
    }

    auto SetLevel(const LogLevel level) -> MessageBuilder &
    {
        message_->level = level;
        return *this;
    }

    auto SetLocation(SourceLocation location) -> MessageBuilder &
    {
        message_->location = std::move(location);
        return *this;
    }

    auto AddContext(std::string key, std::string value) -> MessageBuilder &
    {
        message_->context[std::move(key)] = std::move(value);
        return *this;
    }

    auto SetTraceContext(TraceContext trace_context) -> MessageBuilder &
    {
        message_->trace_context = std::move(trace_context);
        return *this;
    }

    auto SetActorId(const ActorId actor_id) -> MessageBuilder &
    {
        message_->actor_id = actor_id;
        return *this;
    }

    auto SetActorName(std::string actor_name) -> MessageBuilder &
    {
        message_->actor_name = std::move(actor_name);
        return *this;
    }

  private:
    MPtr<LogMessage> message_;
};

template <typename... Args>
auto CreateLogBuilder(const LogLevel level, SourceLocation location, Args &&...args) -> MessageBuilder
{
    return MessageBuilder(level, std::move(location), MakeMessageFormat(std::forward<Args>(args)...));
}

#define LOG_INTERNAL(level, ...) CreateLogBuilder(level, SourceLocation{__FILE__, __LINE__, __FUNCTION__}, __VA_ARGS__)

#define LOG_TRACE(...) LOG_INTERNAL(LogLevel::TRACE, __VA_ARGS__)

#define LOG_DEBUG(...) LOG_INTERNAL(LogLevel::DEBUG, __VA_ARGS__)

#define LOG_INFO(...) LOG_INTERNAL(LogLevel::INFO, __VA_ARGS__)

#define LOG_WARNING(...) LOG_INTERNAL(LogLevel::WARNING, __VA_ARGS__)

#define LOG_ERROR(...) LOG_INTERNAL(LogLevel::ERROR, __VA_ARGS__)

#define LOG_CRITICAL(...) LOG_INTERNAL(LogLevel::CRITICAL, __VA_ARGS__)
} // namespace alarm_manager
