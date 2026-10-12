#pragma once

#include <chrono>
#include <ctime>
#include <string>

namespace soilwatercm::utils
{

    /**
     * @brief 时间工具
     *
     * 秒/毫秒/微秒时间戳与字符串互转、偏移计算、ISO8601 格式化。
     */
    class TimeUtils
    {
    public:
        /// @brief 当前秒级时间戳
        static int64_t now();

        /// @brief 当前毫秒级时间戳
        static int64_t nowMs();

        /// @brief 当前微秒级时间戳
        static int64_t nowUs();

        /// @brief 时间戳按 strftime 格式格式化
        static std::string format(int64_t timestamp, const std::string& format = "%Y-%m-%d %H:%M:%S");

        /// @brief 当前时间格式化
        static std::string formatNow(const std::string& format = "%Y-%m-%d %H:%M:%S");

        /// @brief 按 strftime 格式解析为本地时间时间戳
        static int64_t parse(const std::string& time_str, const std::string& format = "%Y-%m-%d %H:%M:%S");

        /// @brief 时间戳转 ISO8601 字符串
        static std::string toIso8601(int64_t timestamp);

        /// @brief ISO8601 字符串转时间戳
        static int64_t fromIso8601(const std::string& iso_str);

        static int64_t addDays(int64_t timestamp, int days);
        static int64_t addHours(int64_t timestamp, int hours);
        static int64_t addMinutes(int64_t timestamp, int minutes);

        /// @brief 两时间戳相差秒数
        static int64_t diffSeconds(int64_t timestamp1, int64_t timestamp2);

        /// @brief 两时间戳相差天数
        static int64_t diffDays(int64_t timestamp1, int64_t timestamp2);

        /// @brief 是否周六/周日
        static bool isWeekend(int64_t timestamp);

        /// @brief 星期几 (0=周日)
        static int getDayOfWeek(int64_t timestamp);

        /// @brief 当天 0 点时间戳
        static int64_t startOfDay(int64_t timestamp);

        /// @brief 当天最后一秒时间戳
        static int64_t endOfDay(int64_t timestamp);

        /// @brief 当前时区名
        static std::string getCurrentTimezone();
    };

}  // namespace soilwatercm::utils
