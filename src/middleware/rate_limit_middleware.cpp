#include "rate_limit_middleware.h"

#include <drogon/HttpTypes.h>

#include "../utils/logger.h"

namespace soilwatercm::middleware
{

    RateLimitMiddleware::RateLimitMiddleware()
    {
        ipRateLimit_ = {100, 60};
        userRateLimit_ = {200, 60};
    }

    void RateLimitMiddleware::doFilter(const drogon::HttpRequestPtr& req, drogon::FilterCallback&& fcb,
                                       drogon::FilterChainCallback&& fccb)
    {
        cleanupExpiredEntries();

        std::string ip = std::string(req->peerAddr().toIp());

        if (!checkIpRateLimit(ip))
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k429TooManyRequests);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Rate limit exceeded for IP address"})");
            resp->addHeader("Retry-After", std::to_string(ipRateLimit_.window_seconds));
            fcb(resp);
            return;
        }

        std::string userIdStr = std::string(req->getParameter("user_id"));
        if (!userIdStr.empty())
        {
            int userId = std::stoi(userIdStr);

            if (!checkUserRateLimit(userId))
            {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k429TooManyRequests);
                resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                resp->setBody(R"({"error": "Rate limit exceeded for user"})");
                resp->addHeader("Retry-After", std::to_string(userRateLimit_.window_seconds));
                fcb(resp);
                return;
            }
        }

        fccb();
    }

    void RateLimitMiddleware::setIpRateLimit(int maxRequests, int windowSeconds)
    {
        ipRateLimit_.max_requests = maxRequests;
        ipRateLimit_.window_seconds = windowSeconds;
    }

    void RateLimitMiddleware::setUserRateLimit(int maxRequests, int windowSeconds)
    {
        userRateLimit_.max_requests = maxRequests;
        userRateLimit_.window_seconds = windowSeconds;
    }

    bool RateLimitMiddleware::checkIpRateLimit(const std::string& ip)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto now = std::chrono::steady_clock::now();
        auto it = ipCache_.find(ip);

        if (it == ipCache_.end())
        {
            ipCache_[ip] = {1, now};
            return true;
        }

        auto& [count, timestamp] = it->second;
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - timestamp).count();

        if (elapsed >= ipRateLimit_.window_seconds)
        {
            count = 1;
            timestamp = now;
            return true;
        }

        if (count >= ipRateLimit_.max_requests)
        {
            return false;
        }

        count++;
        return true;
    }

    bool RateLimitMiddleware::checkUserRateLimit(int userId)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto now = std::chrono::steady_clock::now();
        auto it = userCache_.find(userId);

        if (it == userCache_.end())
        {
            userCache_[userId] = {1, now};
            return true;
        }

        auto& [count, timestamp] = it->second;
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - timestamp).count();

        if (elapsed >= userRateLimit_.window_seconds)
        {
            count = 1;
            timestamp = now;
            return true;
        }

        if (count >= userRateLimit_.max_requests)
        {
            return false;
        }

        count++;
        return true;
    }

    void RateLimitMiddleware::cleanupExpiredEntries()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto now = std::chrono::steady_clock::now();

        for (auto it = ipCache_.begin(); it != ipCache_.end();)
        {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.second).count();

            if (elapsed >= ipRateLimit_.window_seconds * 2)
            {
                it = ipCache_.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = userCache_.begin(); it != userCache_.end();)
        {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.second).count();

            if (elapsed >= userRateLimit_.window_seconds * 2)
            {
                it = userCache_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

}  // namespace soilwatercm::middleware
