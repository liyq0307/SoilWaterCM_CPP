#pragma once

#include <chrono>
#include <string>

#include <drogon/HttpFilter.h>

namespace soilwatercm::middleware
{

    /**
     * @brief 请求日志过滤器
     *
     * 生成 trace_id, 记录请求进入和完成耗时。
     */
    class LoggingMiddleware : public drogon::HttpFilter<LoggingMiddleware, false>
    {
    public:
        LoggingMiddleware();

        /**
         * @brief 注入 trace_id 请求参数并记录访问日志
         *
         * 放行前后各打一条 info 日志 (方法/路径/来源与完成耗时)。
         */
        void doFilter(const drogon::HttpRequestPtr& req, drogon::FilterCallback&& fcb,
                      drogon::FilterChainCallback&& fccb) override;

    private:
        /// @brief 生成 32 位十六进制随机追踪号
        std::string generateTraceId();
    };

}  // namespace soilwatercm::middleware
