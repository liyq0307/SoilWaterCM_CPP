#pragma once

#include <set>
#include <string>

#include <drogon/HttpFilter.h>

#include "../models/common/enums.h"

namespace soilwatercm::middleware
{

    /**
     * @brief 角色权限过滤器
     *
     * 按请求参数中的 user_role (由 JwtMiddleware 注入) 判断能否访问。
     */
    class AuthMiddleware : public drogon::HttpFilter<AuthMiddleware, false>
    {
    public:
        // @brief 构造, 默认放行 Admin/Monitor/Leader 三种角色
        AuthMiddleware();

        /**
         * @brief 校验请求参数 user_id 与 user_role
         *
         * 缺少用户信息返回 401, 角色不在允许列表返回 403; Admin 始终放行。
         */
        void doFilter(const drogon::HttpRequestPtr& req, drogon::FilterCallback&& fcb,
                      drogon::FilterChainCallback&& fccb) override;

        /**
         * @brief 设置允许访问的角色 (覆盖现有列表)
         * @param role 允许的角色; Admin 会额外放行 Monitor/Leader
         */
        void requireRole(models::UserRole role);

        // @brief 追加要求的权限名 (当前 doFilter 未校验权限列表)
        void requirePermission(const std::string& permission);

    private:
        std::set<models::UserRole> allowedRoles_;
        std::set<std::string> requiredPermissions_;

        bool hasRequiredRole(models::UserRole userRole);
        bool hasRequiredPermissions(models::UserRole userRole);
    };

}  // namespace soilwatercm::middleware
