#include "jwt_middleware.h"

#include <drogon/HttpTypes.h>

#include "../utils/logger.h"

namespace soilwatercm::middleware
{

    JwtMiddleware::JwtMiddleware(std::shared_ptr<services::AuthService> authService)
        : authService_(std::move(authService))
    {
    }

    void JwtMiddleware::doFilter(const drogon::HttpRequestPtr& req, drogon::FilterCallback&& fcb,
                                 drogon::FilterChainCallback&& fccb)
    {
        std::string path = std::string(req->path());

        if (isExcludedPath(path))
        {
            fccb();
            return;
        }

        auto tokenOpt = extractToken(req);
        if (!tokenOpt)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k401Unauthorized);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Missing authentication token"})");
            fcb(resp);
            return;
        }

        auto userIdOpt = authService_->validateToken(*tokenOpt);
        if (!userIdOpt)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k401Unauthorized);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Invalid or expired token"})");
            fcb(resp);
            return;
        }

        auto userOpt = authService_->getUserFromToken(*tokenOpt);
        if (!userOpt)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k401Unauthorized);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "User not found"})");
            fcb(resp);
            return;
        }

        req->setParameter("user_id", std::to_string(*userIdOpt));
        req->setParameter("user_role", std::to_string(static_cast<int>(userOpt->role)));

        fccb();
    }

    std::optional<std::string> JwtMiddleware::extractToken(const drogon::HttpRequestPtr& req)
    {
        std::string authHeader = std::string(req->getHeader("Authorization"));

        if (authHeader.empty())
        {
            return std::nullopt;
        }

        const std::string prefix = "Bearer ";
        if (authHeader.substr(0, prefix.size()) != prefix)
        {
            return std::nullopt;
        }

        return authHeader.substr(prefix.size());
    }

    bool JwtMiddleware::isExcludedPath(const std::string& path)
    {
        static const std::vector<std::string> excludedPaths = {"/api/v1/auth/login", "/api/v1/auth/register", "/health",
                                                               "/metrics"};

        for (const auto& excluded : excludedPaths)
        {
            if (path == excluded)
            {
                return true;
            }
        }

        return false;
    }

}  // namespace soilwatercm::middleware
