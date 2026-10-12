#include "auth_middleware.h"

#include <drogon/HttpTypes.h>

#include "../utils/logger.h"

namespace soilwatercm::middleware
{

    AuthMiddleware::AuthMiddleware()
    {
        allowedRoles_ = {models::UserRole::Admin, models::UserRole::Monitor, models::UserRole::Leader};
    }

    void AuthMiddleware::doFilter(const drogon::HttpRequestPtr& req, drogon::FilterCallback&& fcb,
                                  drogon::FilterChainCallback&& fccb)
    {
        std::string userIdStr = std::string(req->getParameter("user_id"));
        std::string userRoleStr = std::string(req->getParameter("user_role"));

        if (userIdStr.empty() || userRoleStr.empty())
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k401Unauthorized);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Authentication required"})");
            fcb(resp);
            return;
        }

        int roleInt = std::stoi(userRoleStr);
        models::UserRole userRole = static_cast<models::UserRole>(roleInt);

        if (!hasRequiredRole(userRole))
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k403Forbidden);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Insufficient permissions"})");
            fcb(resp);
            return;
        }

        fccb();
    }

    void AuthMiddleware::requireRole(models::UserRole role)
    {
        allowedRoles_.clear();
        allowedRoles_.insert(role);

        if (role == models::UserRole::Admin)
        {
            allowedRoles_.insert(models::UserRole::Monitor);
            allowedRoles_.insert(models::UserRole::Leader);
        }
    }

    void AuthMiddleware::requirePermission(const std::string& permission)
    {
        requiredPermissions_.insert(permission);
    }

    bool AuthMiddleware::hasRequiredRole(models::UserRole userRole)
    {
        if (allowedRoles_.empty())
        {
            return true;
        }

        if (userRole == models::UserRole::Admin)
        {
            return true;
        }

        return allowedRoles_.find(userRole) != allowedRoles_.end();
    }

    bool AuthMiddleware::hasRequiredPermissions(models::UserRole userRole)
    {
        if (userRole == models::UserRole::Admin)
        {
            return true;
        }

        return true;
    }

}  // namespace soilwatercm::middleware
