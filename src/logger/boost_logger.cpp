#include <oxherdcpp/logger/boost_logger.h>

#include <filesystem>
#include <utility>

#include <boost/json.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/exception_handler.hpp>
#include <boost/log/utility/setup.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/phoenix/bind.hpp>

namespace oxherdcpp::boost_logger
{

namespace logging = boost::log;
namespace expr = logging::expressions;
namespace keywords = logging::keywords;
namespace phoenix = boost::phoenix;
namespace json = boost::json;

namespace
{
boost::shared_ptr<logging::sinks::synchronous_sink<logging::sinks::text_ostream_backend>> console_sink;
boost::shared_ptr<logging::sinks::synchronous_sink<logging::sinks::text_file_backend>> file_sink;

auto FormatTimeISO8601(const std::chrono::system_clock::time_point &tp) -> std::string
{
    const auto itt = std::chrono::system_clock::to_time_t(tp);
    std::ostringstream ss;
    ss << std::put_time(gmtime(&itt), "%FT%T");

    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch()) % 1000000000;
    ss << '.' << std::setfill('0') << std::setw(9) << ns.count() << 'Z';

    return ss.str();
}

} // namespace

BoostLogger::BoostLogger(const std::string &channel_name, LoggerConfig config)
    : channel_name_{channel_name}, config_{std::move(config)}, logger_{keywords::channel = channel_name},
      current_level_{config_.log_level}
{
    logger_.add_attribute("Channel", boost::log::attributes::constant(channel_name));
}

auto BoostLogger::Log(const MPtr<LogMessage> &log_message) -> void
{
    if (log_message->level < current_level_)
    {
        return;
    }

    if (auto rec = logger_.open_record(keywords::severity = ConvertLogLevel(log_message->level)))
    {
        logging::record_ostream strm{rec};

        if (config_.enable_json_format)
        {
            json::object log_obj;
            log_obj["timestamp"] = FormatTimeISO8601(log_message->timestamp);
            log_obj["level"] = logging::trivial::to_string(ConvertLogLevel(log_message->level));
            log_obj["message"] = log_message->message;

            log_obj["service"] = {{"name", "alarm-manager-ecs"}, {"version", "0.1.0"}};

            log_obj["source"] = {{"file", log_message->location.file.data()},
                                 {"line", log_message->location.line},
                                 {"function", log_message->location.function.data()}};

            log_obj["actor"] = {{"id", log_message->actor_id}, {"name", log_message->actor_name}};

            log_obj["trace"] = {{"trace_id", log_message->trace_context.trace_id},
                                {"span_id", log_message->trace_context.span_id}};

            json::object context_obj;
            for (const auto &[key, value] : log_message->context)
            {
                context_obj[key] = value;
            }
            log_obj["context"] = std::move(context_obj);

            strm << json::serialize(log_obj);
        }
        else
        {
            strm << "[" << log_message->actor_name << ":" << log_message->actor_id << "] ";
            strm << log_message->message;
            if (!log_message->context.empty())
            {
                strm << " {";
                for (const auto &[key, value] : log_message->context)
                {
                    strm << " " << key << "=" << value;
                }
                strm << " }";
            }
        }
        strm.flush();
        logger_.push_record(std::move(rec));
    }
}

auto BoostLogger::Flush() -> void
{
    boost::log::core::get()->flush();
}

auto BoostLogger::ApplyConfig(const LoggerConfig &config) -> void
{
    config_ = config;
    UpdateFilters();
}

auto BoostLogger::SetLevel(const LogLevel level) -> void
{
    current_level_ = level;
    logging::core::get()->set_filter(boost::log::trivial::severity >= ConvertLogLevel(level));
}

auto BoostLogger::GetLevel() const -> LogLevel
{
    return current_level_;
}

auto BoostLogger::ConvertLogLevel(const LogLevel level) -> boost::log::trivial::severity_level
{
    switch (level)
    {
    case LogLevel::TRACE: {
        return boost::log::trivial::trace;
    }
    case LogLevel::DEBUG: {
        return boost::log::trivial::debug;
    }
    case LogLevel::INFO: {
        return boost::log::trivial::info;
    }
    case LogLevel::WARNING: {
        return boost::log::trivial::warning;
    }
    case LogLevel::ERROR: {
        return boost::log::trivial::error;
    }
    case LogLevel::CRITICAL: {
        return boost::log::trivial::fatal;
    }
    default:;
        return boost::log::trivial::info;
    }
}

auto BoostLogger::ConvertLogLevel(const boost::log::trivial::severity_level level) -> LogLevel
{
    switch (level)
    {
    case boost::log::trivial::trace: {
        return LogLevel::TRACE;
    }
    case boost::log::trivial::debug: {
        return LogLevel::DEBUG;
    }
    case boost::log::trivial::info: {
        return LogLevel::INFO;
    }
    case boost::log::trivial::warning: {
        return LogLevel::WARNING;
    }
    case boost::log::trivial::error: {
        return LogLevel::ERROR;
    }
    case boost::log::trivial::fatal: {
        return LogLevel::CRITICAL;
    }
    default:;
    }
    return LogLevel::INFO;
}

auto BoostLogger::UpdateFilters() const -> void
{
    auto level_filter = logging::trivial::severity >= ConvertLogLevel(config_.log_level);

    auto channel_filter = [this](const logging::value_ref<std::string> &channel) {
        if (!channel)
        {
            return false;
        }
        for (const auto &excluded : config_.excluded_channels)
        {
            if (channel.get() == excluded)
                return false;
        }
        if (!config_.included_channels.empty())
        {
            return std::ranges::any_of(config_.included_channels,
                                       [&](const auto &included) { return channel.get() == included; });
        }

        return true;
    };
    if (console_sink)
    {
        console_sink->set_filter(level_filter && phoenix::bind(channel_filter, expr::attr<std::string>("Channel")));
    }
    if (file_sink)
    {
        file_sink->set_filter(level_filter && phoenix::bind(channel_filter, expr::attr<std::string>("Channel")));
    }
}

auto InitBoostLogger(const LoggerConfig &config) -> void
{
    namespace sinks = boost::log::sinks;

    if (config.enable_file && !config.log_file_path.empty())
    {
        std::filesystem::create_directories(config.log_file_path);
    }

    boost::log::formatter fmt;
    if (config.enable_json_format)
    {
        fmt = expr::stream << expr::smessage;
    }
    else
    {
        fmt = expr::stream << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                           << " [" << logging::trivial::severity << "]"
                           << " [" << expr::attr<std::string>("Channel") << "] " << expr::smessage;
    }
    if (config.enable_console)
    {
        console_sink = logging::add_console_log(std::clog, keywords::format = fmt);
    }
    if (config.enable_file)
    {
        auto full_path =
            std::filesystem::path(config.log_file_path) / (config.log_file_name + "_%Y-%m-%d_%H-%M-%S.log");
        auto backend = boost::make_shared<sinks::text_file_backend>(keywords::file_name = full_path,
                                                                    keywords::rotation_size = config.rotation_size);

        if (config.rotation_daily)
        {
            backend->set_time_based_rotation(sinks::file::rotation_at_time_point(0, 0, 0));
        }
        else
        {
            backend->set_time_based_rotation(sinks::file::rotation_at_time_interval(boost::posix_time::hours(24)));
        }
        file_sink = boost::make_shared<sinks::synchronous_sink<sinks::text_file_backend>>(backend);
        file_sink->set_formatter(fmt);

        logging::core::get()->add_sink(file_sink);
    }
    logging::core::get()->set_exception_handler(logging::make_exception_suppressor());
    logging::add_common_attributes();
}

} // namespace oxherdcpp::boost_logger