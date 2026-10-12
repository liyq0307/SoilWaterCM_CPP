#include "logging_middleware.h"

#include <iomanip>
#include <random>
#include <sstream>

#include "../utils/logger.h"

namespace soilwatercm::middleware
{

    LoggingMiddleware::LoggingMiddleware() {}

    void LoggingMiddleware::doFilter(const drogon::HttpRequestPtr& req, drogon::FilterCallback&& fcb,
                                     drogon::FilterChainCallback&& fccb)
    {
        (void)fcb;
        std::string traceId = generateTraceId();
        req->setParameter("trace_id", traceId);

        auto startTime = std::chrono::steady_clock::now();

        std::string method = std::string(req->methodString());
        std::string path = std::string(req->path());
        std::string remoteAddr = std::string(req->peerAddr().toIpPort());

        utils::Logger::info("[{}] {} {} from {}", traceId, method, path, remoteAddr);

        fccb();

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        utils::Logger::info("[{}] Request completed in {} ms", traceId, duration.count());
    }

    std::string LoggingMiddleware::generateTraceId()
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);

        std::stringstream ss;
        ss << std::hex;

        for (int i = 0; i < 32; ++i)
        {
            ss << dis(gen);
        }

        return ss.str();
    }

}  // namespace soilwatercm::middleware
