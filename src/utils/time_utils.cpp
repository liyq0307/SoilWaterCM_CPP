#define _CRT_SECURE_NO_WARNINGS
#include "time_utils.h"

#include <iomanip>
#include <sstream>

namespace soilwatercm::utils
{

    int64_t TimeUtils::now()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    int64_t TimeUtils::nowMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    int64_t TimeUtils::nowUs()
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    std::string TimeUtils::format(int64_t timestamp, const std::string& format)
    {
        std::time_t time = static_cast<std::time_t>(timestamp);
        std::tm* tm = std::localtime(&time);

        std::ostringstream oss;
        oss << std::put_time(tm, format.c_str());
        return oss.str();
    }

    std::string TimeUtils::formatNow(const std::string& formatStr)
    {
        return TimeUtils::format(now(), formatStr);
    }

    int64_t TimeUtils::parse(const std::string& time_str, const std::string& format)
    {
        std::tm tm = {};
        std::istringstream iss(time_str);
        iss >> std::get_time(&tm, format.c_str());

        return static_cast<int64_t>(std::mktime(&tm));
    }

    std::string TimeUtils::toIso8601(int64_t timestamp)
    {
        return format(timestamp, "%Y-%m-%dT%H:%M:%SZ");
    }

    int64_t TimeUtils::fromIso8601(const std::string& iso_str)
    {
        return parse(iso_str, "%Y-%m-%dT%H:%M:%SZ");
    }

    int64_t TimeUtils::addDays(int64_t timestamp, int days)
    {
        return timestamp + static_cast<int64_t>(days) * 86400;
    }

    int64_t TimeUtils::addHours(int64_t timestamp, int hours)
    {
        return timestamp + static_cast<int64_t>(hours) * 3600;
    }

    int64_t TimeUtils::addMinutes(int64_t timestamp, int minutes)
    {
        return timestamp + static_cast<int64_t>(minutes) * 60;
    }

    int64_t TimeUtils::diffSeconds(int64_t timestamp1, int64_t timestamp2)
    {
        return timestamp1 - timestamp2;
    }

    int64_t TimeUtils::diffDays(int64_t timestamp1, int64_t timestamp2)
    {
        return diffSeconds(timestamp1, timestamp2) / 86400;
    }

    bool TimeUtils::isWeekend(int64_t timestamp)
    {
        int day = getDayOfWeek(timestamp);
        return day == 0 || day == 6;
    }

    int TimeUtils::getDayOfWeek(int64_t timestamp)
    {
        std::time_t time = static_cast<std::time_t>(timestamp);
        std::tm* tm = std::localtime(&time);
        return tm->tm_wday;
    }

    int64_t TimeUtils::startOfDay(int64_t timestamp)
    {
        std::time_t time = static_cast<std::time_t>(timestamp);
        std::tm* tm = std::localtime(&time);

        tm->tm_hour = 0;
        tm->tm_min = 0;
        tm->tm_sec = 0;

        return static_cast<int64_t>(std::mktime(tm));
    }

    int64_t TimeUtils::endOfDay(int64_t timestamp)
    {
        std::time_t time = static_cast<std::time_t>(timestamp);
        std::tm* tm = std::localtime(&time);

        tm->tm_hour = 23;
        tm->tm_min = 59;
        tm->tm_sec = 59;

        return static_cast<int64_t>(std::mktime(tm));
    }

    std::string TimeUtils::getCurrentTimezone()
    {
        std::time_t time = std::time(nullptr);
        std::tm* tm = std::localtime(&time);

        char buffer[64];
        strftime(buffer, sizeof(buffer), "%Z", tm);
        return std::string(buffer);
    }

}  // namespace soilwatercm::utils
