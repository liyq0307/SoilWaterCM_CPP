#pragma once

#include <memory>
#include <string>
#include <vector>

#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#ifdef ERROR
#undef ERROR
#endif
#ifdef INFO
#undef INFO
#endif
#ifdef DEBUG
#undef DEBUG
#endif

namespace soilwatercm::utils
{

    /// 日志级别
    enum class LogLevel
    {
        DEBUG = 0,
        INFO = 1,
        WARNING = 2,
        ERROR = 3,
        CRITICAL = 4
    };

    /// 日志配置
    struct LoggerConfig
    {
        std::string name = "soilwatercm";
        LogLevel level = LogLevel::INFO;
        bool console_enabled = true;
        bool file_enabled = true;
        std::string log_dir = "logs";
        std::string log_file = "soilwatercm.log";
        size_t max_file_size = 10485760;
        size_t max_files = 30;
        bool rotate_on_open = true;
        std::string pattern =
            "{\"timestamp\":\"%Y-%m-%d "
            "%H:%M:%S.%f\",\"level\":\"%l\",\"logger\":\"%n\",\"message\":\"%v\",\"thread_id\":\"%t\",\"trace_id\":\"%"
            "x\"}";
    };

    /**
     * @brief spdlog 封装
     *
     * 控制台 + 轮转文件双输出, 日志 pattern 带 trace_id 便于串联请求。
     */
    class Logger
    {
    public:
        /// @brief 全局唯一实例
        static Logger& instance();

        /// @brief 按配置初始化日志器
        void initialize(const LoggerConfig& config);

        /// @brief 忽略传入的文件路径, 直接用内置默认配置初始化
        void initialize(const std::string& config_file);

        /// @brief 设置输出级别
        void setLevel(LogLevel level);

        /// @brief 当前输出级别
        LogLevel getLevel() const;

        // trace_id 写入后随每条日志输出, 供跨模块追踪同一请求

        /// @brief 设置追踪号
        void setTraceId(const std::string& trace_id);

        /// @brief 清除追踪号
        void clearTraceId();

        template <typename... Args>
        void logDebug(fmt::format_string<Args...> fmt, Args&&... args)
        {
            logger_->debug(std::move(fmt), std::forward<Args>(args)...);
        }

        template <typename... Args>
        void logInfo(fmt::format_string<Args...> fmt, Args&&... args)
        {
            logger_->info(std::move(fmt), std::forward<Args>(args)...);
        }

        template <typename... Args>
        void logWarn(fmt::format_string<Args...> fmt, Args&&... args)
        {
            logger_->warn(std::move(fmt), std::forward<Args>(args)...);
        }

        template <typename... Args>
        void logError(fmt::format_string<Args...> fmt, Args&&... args)
        {
            logger_->error(std::move(fmt), std::forward<Args>(args)...);
        }

        template <typename... Args>
        void logCritical(fmt::format_string<Args...> fmt, Args&&... args)
        {
            logger_->critical(std::move(fmt), std::forward<Args>(args)...);
        }

        /// @brief 字符串消息版日志接口 (不做格式化)
        void logDebugMsg(const std::string& msg);
        void logInfoMsg(const std::string& msg);
        void logWarnMsg(const std::string& msg);
        void logErrorMsg(const std::string& msg);
        void logCriticalMsg(const std::string& msg);

        // ---- 静态便捷接口 ----
        template <typename... Args>
        static void info(fmt::format_string<Args...> fmt, Args&&... args)
        {
            instance().logInfo(std::move(fmt), std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void debug(fmt::format_string<Args...> fmt, Args&&... args)
        {
            instance().logDebug(std::move(fmt), std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void warn(fmt::format_string<Args...> fmt, Args&&... args)
        {
            instance().logWarn(std::move(fmt), std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void error(fmt::format_string<Args...> fmt, Args&&... args)
        {
            instance().logError(std::move(fmt), std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void critical(fmt::format_string<Args...> fmt, Args&&... args)
        {
            instance().logCritical(std::move(fmt), std::forward<Args>(args)...);
        }

        /// @brief 便捷初始化: 指定日志文件/级别名/是否控制台输出
        static void init(const std::string& logFile, const std::string& level, bool console)
        {
            LoggerConfig config;
            config.log_file = logFile;
            if (level == "DEBUG")
                config.level = LogLevel::DEBUG;
            else if (level == "INFO")
                config.level = LogLevel::INFO;
            else if (level == "WARNING" || level == "WARN")
                config.level = LogLevel::WARNING;
            else if (level == "ERROR")
                config.level = LogLevel::ERROR;
            else if (level == "CRITICAL")
                config.level = LogLevel::CRITICAL;
            config.console_enabled = console;
            instance().initialize(config);
        }

        /// @brief 按名取 spdlog 记录器, 不存在则新建一个控制台彩色记录器
        static std::shared_ptr<spdlog::logger> getLogger(const std::string& name)
        {
            auto logger = spdlog::get(name);
            if (!logger)
            {
                logger = spdlog::stdout_color_mt(name);
            }
            return logger;
        }

        /// @brief 立即刷盘
        void flush();

        /// @brief 主日志器
        std::shared_ptr<spdlog::logger> getLogger() const { return logger_; }

        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        Logger(Logger&&) = delete;
        Logger& operator=(Logger&&) = delete;

    private:
        Logger();
        ~Logger() = default;

        spdlog::level::level_enum toSpdlogLevel(LogLevel level) const;
        LogLevel fromSpdlogLevel(spdlog::level::level_enum level) const;
        void createLogDirectory(const std::string& dir);

        std::shared_ptr<spdlog::logger> logger_;
        LoggerConfig config_;
        std::string trace_id_;
    };

#ifndef LOG_DEBUG
#define LOG_DEBUG(...) soilwatercm::utils::Logger::instance().logDebugMsg(__VA_ARGS__)
#endif
#ifndef LOG_INFO
#define LOG_INFO(...) soilwatercm::utils::Logger::instance().logInfoMsg(__VA_ARGS__)
#endif
#ifndef LOG_WARN
#define LOG_WARN(...) soilwatercm::utils::Logger::instance().logWarnMsg(__VA_ARGS__)
#endif
#ifndef LOG_ERROR
#define LOG_ERROR(...) soilwatercm::utils::Logger::instance().logErrorMsg(__VA_ARGS__)
#endif
#ifndef LOG_CRITICAL
#define LOG_CRITICAL(...) soilwatercm::utils::Logger::instance().logCriticalMsg(__VA_ARGS__)
#endif

#ifndef LOG_SET_TRACE_ID
#define LOG_SET_TRACE_ID(id) soilwatercm::utils::Logger::instance().setTraceId(id)
#endif
#ifndef LOG_CLEAR_TRACE_ID
#define LOG_CLEAR_TRACE_ID() soilwatercm::utils::Logger::instance().clearTraceId()
#endif

}  // namespace soilwatercm::utils
