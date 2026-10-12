#include "logger.h"

#include <filesystem>
#include <stdexcept>

namespace soilwatercm::utils
{

    Logger& Logger::instance()
    {
        static Logger instance;
        return instance;
    }

    Logger::Logger() {}

    void Logger::initialize(const LoggerConfig& config)
    {
        config_ = config;

        if (config_.file_enabled)
        {
            createLogDirectory(config_.log_dir);
        }

        std::vector<spdlog::sink_ptr> sinks;

        if (config_.console_enabled)
        {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern(config_.pattern);
            sinks.push_back(console_sink);
        }

        if (config_.file_enabled)
        {
            std::string log_path = config_.log_dir + "/" + config_.log_file;

            if (config_.rotate_on_open)
            {
                auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
                    log_path, 0, static_cast<int>(config_.max_files));
                file_sink->set_pattern(config_.pattern);
                sinks.push_back(file_sink);
            }
            else
            {
                auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_path, config_.max_file_size,
                                                                                        config_.max_files);
                file_sink->set_pattern(config_.pattern);
                sinks.push_back(file_sink);
            }
        }

        logger_ = std::make_shared<spdlog::logger>(config_.name, sinks.begin(), sinks.end());
        logger_->set_level(toSpdlogLevel(config_.level));
        logger_->flush_on(spdlog::level::warn);

        spdlog::register_logger(logger_);
        spdlog::set_default_logger(logger_);
    }

    void Logger::initialize(const std::string& /*config_file*/)
    {
        LoggerConfig config;
        config.name = "soilwatercm";
        config.level = LogLevel::INFO;
        config.console_enabled = true;
        config.file_enabled = true;
        config.log_dir = "logs";
        config.log_file = "soilwatercm.log";
        config.max_file_size = 10485760;
        config.max_files = 30;
        config.rotate_on_open = true;

        initialize(config);
    }

    void Logger::setLevel(LogLevel level)
    {
        config_.level = level;
        logger_->set_level(toSpdlogLevel(level));
    }

    LogLevel Logger::getLevel() const
    {
        return config_.level;
    }

    void Logger::setTraceId(const std::string& trace_id)
    {
        trace_id_ = trace_id;
        spdlog::set_default_logger(logger_);
    }

    void Logger::clearTraceId()
    {
        trace_id_.clear();
    }

    void Logger::logDebugMsg(const std::string& msg)
    {
        logger_->debug(msg);
    }

    void Logger::logInfoMsg(const std::string& msg)
    {
        logger_->info(msg);
    }

    void Logger::logWarnMsg(const std::string& msg)
    {
        logger_->warn(msg);
    }

    void Logger::logErrorMsg(const std::string& msg)
    {
        logger_->error(msg);
    }

    void Logger::logCriticalMsg(const std::string& msg)
    {
        logger_->critical(msg);
    }

    void Logger::flush()
    {
        logger_->flush();
    }

    spdlog::level::level_enum Logger::toSpdlogLevel(LogLevel level) const
    {
        switch (level)
        {
            case LogLevel::DEBUG:
                return spdlog::level::debug;
            case LogLevel::INFO:
                return spdlog::level::info;
            case LogLevel::WARNING:
                return spdlog::level::warn;
            case LogLevel::ERROR:
                return spdlog::level::err;
            case LogLevel::CRITICAL:
                return spdlog::level::critical;
            default:
                return spdlog::level::info;
        }
    }

    LogLevel Logger::fromSpdlogLevel(spdlog::level::level_enum level) const
    {
        switch (level)
        {
            case spdlog::level::debug:
                return LogLevel::DEBUG;
            case spdlog::level::info:
                return LogLevel::INFO;
            case spdlog::level::warn:
                return LogLevel::WARNING;
            case spdlog::level::err:
                return LogLevel::ERROR;
            case spdlog::level::critical:
                return LogLevel::CRITICAL;
            default:
                return LogLevel::INFO;
        }
    }

    void Logger::createLogDirectory(const std::string& dir)
    {
        std::filesystem::path log_path(dir);
        if (!std::filesystem::exists(log_path))
        {
            std::filesystem::create_directories(log_path);
        }
    }

}  // namespace soilwatercm::utils
