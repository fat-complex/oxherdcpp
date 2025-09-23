#pragma once

#include <vector>

#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/trivial.hpp>

#include <oxherdcpp/common/helper_macros.h>
#include <oxherdcpp/logger/logger.h>

namespace oxherdcpp::boost_logger
{
struct LoggerConfig
{
    LogLevel log_level{LogLevel::INFO};
    bool enable_console{true};
    bool enable_file{true};
    std::string log_file_path{"./"};
    std::string log_file_name{"app"};
    std::size_t rotation_size{10 * 1024 * 1024};
    bool rotation_daily{true};
    std::string format{"[%TimeStamp%] [%Severity%] [%Channel%] %Message%"};

    bool enable_json_format{false};

    std::vector<std::string> included_channels{};
    std::vector<std::string> excluded_channels{};
};

inline auto LoadLoggerConfig(const std::string &config_path = {}) -> LoggerConfig
{
    (void)config_path;
    return {};
}

inline auto SaveLoggerConfig(const LoggerConfig &config, const std::string &config_path = {}) -> void
{
    (void)config;
    (void)config_path;
}

class BoostLogger final : public Logger
{
  public:
    DISABLE_COPY_AND_MOVE(BoostLogger)

    explicit BoostLogger(const std::string &channel_name, LoggerConfig config);

    ~BoostLogger() override = default;

    auto Log(const MPtr<LogMessage> &log_message) -> void override;

    auto Flush() -> void override;

    auto ApplyConfig(const LoggerConfig &config) -> void;

    auto SetLevel(LogLevel level) -> void override;

    [[nodiscard]] auto GetLevel() const -> LogLevel override;

  private:
    static auto ConvertLogLevel(LogLevel level) -> boost::log::trivial::severity_level;

    static auto ConvertLogLevel(boost::log::trivial::severity_level level) -> LogLevel;

    auto UpdateFilters() const -> void;

    std::string channel_name_;
    LoggerConfig config_;
    boost::log::sources::severity_channel_logger_mt<boost::log::trivial::severity_level> logger_;
    LogLevel current_level_{LogLevel::INFO};
};

auto InitBoostLogger(const LoggerConfig &config) -> void;
} // namespace oxherdcpp::boost_logger