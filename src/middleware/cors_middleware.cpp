#include "cors_middleware.h"

#include "../utils/logger.h"

namespace soilwatercm::middleware
{

    CorsMiddleware::CorsMiddleware()
    {
        allowedOrigins_ = {"*"};
        allowedMethods_ = {"GET", "POST", "PUT", "DELETE", "OPTIONS", "PATCH"};
        allowedHeaders_ = {"Content-Type", "Authorization", "X-Requested-With", "X-Trace-Id"};
    }

    void CorsMiddleware::doFilter(const drogon::HttpRequestPtr& req, drogon::FilterCallback&& fcb,
                                  drogon::FilterChainCallback&& fccb)
    {
        std::string method = std::string(req->methodString());

        if (method == "OPTIONS")
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k204NoContent);

            std::string origin = std::string(req->getHeader("Origin"));
            if (isOriginAllowed(origin))
            {
                resp->addHeader("Access-Control-Allow-Origin", origin);
            }
            else if (!allowedOrigins_.empty() && allowedOrigins_[0] == "*")
            {
                resp->addHeader("Access-Control-Allow-Origin", "*");
            }

            resp->addHeader("Access-Control-Allow-Methods", joinVector(allowedMethods_, ", "));
            resp->addHeader("Access-Control-Allow-Headers", joinVector(allowedHeaders_, ", "));

            if (allowCredentials_)
            {
                resp->addHeader("Access-Control-Allow-Credentials", "true");
            }

            resp->addHeader("Access-Control-Max-Age", std::to_string(maxAge_));

            fcb(resp);
            return;
        }

        fccb();
    }

    void CorsMiddleware::setAllowedOrigins(const std::vector<std::string>& origins)
    {
        allowedOrigins_ = origins;
    }

    void CorsMiddleware::setAllowedMethods(const std::vector<std::string>& methods)
    {
        allowedMethods_ = methods;
    }

    void CorsMiddleware::setAllowedHeaders(const std::vector<std::string>& headers)
    {
        allowedHeaders_ = headers;
    }

    void CorsMiddleware::setAllowCredentials(bool allow)
    {
        allowCredentials_ = allow;
    }

    void CorsMiddleware::setMaxAge(int seconds)
    {
        maxAge_ = seconds;
    }

    bool CorsMiddleware::isOriginAllowed(const std::string& origin)
    {
        for (const auto& allowed : allowedOrigins_)
        {
            if (allowed == "*" || allowed == origin)
            {
                return true;
            }
        }
        return false;
    }

    std::string CorsMiddleware::joinVector(const std::vector<std::string>& vec, const std::string& delimiter)
    {
        std::string result;
        for (size_t i = 0; i < vec.size(); ++i)
        {
            if (i > 0)
            {
                result += delimiter;
            }
            result += vec[i];
        }
        return result;
    }

}  // namespace soilwatercm::middleware
