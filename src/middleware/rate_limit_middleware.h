#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <string>

#include <drogon/HttpFilter.h>

namespace soilwatercm::middleware
{

    /// 限流窗口配置
    struct RateLimitConfig
    {
        int max_requests = 100;
        int window_seconds = 60;
    };

    /**
     * @brief 限流过滤器
     *
     * 按 IP 和登录用户分别计数, 固定窗口内超限返回 429。
     */
    class RateLimitMiddleware : public drogon::HttpFilter<RateLimitMiddleware, false>
    {
    public:
        /// @brief 构造, IP 默认 60 秒 100 次, 用户默认 60 秒 200 次
        RateLimitMiddleware();

        /**
         * @brief 固定窗口限流
         *
         * 先清理过期计数, 再查 IP 限额; 请求参数带 user_id 时再查用户限额。
         * 任一超限返回 429 并带 Retry-After 头。
         */
        void doFilter(const drogon::HttpRequestPtr& req, drogon::FilterCallback&& fcb,
                      drogon::FilterChainCallback&& fccb) override;

        // 调整各自的窗口大小与请求上限

        /// @brief 设置 IP 限流参数
        void setIpRateLimit(int maxRequests, int windowSeconds);

        /// @brief 设置用户限流参数
        void setUserRateLimit(int maxRequests, int windowSeconds);

    private:
        RateLimitConfig ipRateLimit_;
        RateLimitConfig userRateLimit_;

        std::map<std::string, std::pair<int, std::chrono::steady_clock::time_point>> ipCache_;
        std::map<int, std::pair<int, std::chrono::steady_clock::time_point>> userCache_;
        std::mutex mutex_;

        /// @brief IP 窗口内计数是否未超限 (超限返回 false, 否则计数 +1)
        bool checkIpRateLimit(const std::string& ip);

        /// @brief 用户窗口内计数是否未超限
        bool checkUserRateLimit(int userId);

        /// @brief 清理超过两倍窗口的过期计数
        void cleanupExpiredEntries();
    };

}  // namespace soilwatercm::middleware
