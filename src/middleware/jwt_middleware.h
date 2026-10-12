#pragma once

#include <optional>
#include <string>

#include <drogon/HttpFilter.h>

#include "../services/auth_service.h"

namespace soilwatercm::middleware
{

    /**
     * @brief JWT 认证过滤器
     *
     * 从 Authorization 头取 Bearer token 校验, 通过后把 user_id/role 塞进请求参数。
     */
    class JwtMiddleware : public drogon::HttpFilter<JwtMiddleware, false>
    {
    public:
        /**
         * @brief 构造, 注入认证服务
         * @param authService Token 校验与用户解析
         */
        explicit JwtMiddleware(std::shared_ptr<services::AuthService> authService);

        /**
         * @brief 校验请求携带的 JWT
         *
         * 登录/注册/健康检查/指标等路径直接放行; 其余无 token、token 无效
         * 或用户不存在均返回 401, 通过后注入 user_id 与 user_role 请求参数。
         */
        void doFilter(const drogon::HttpRequestPtr& req, drogon::FilterCallback&& fcb,
                      drogon::FilterChainCallback&& fccb) override;

    private:
        std::shared_ptr<services::AuthService> authService_;

        /// @brief 从 Authorization 头提取 Bearer token
        std::optional<std::string> extractToken(const drogon::HttpRequestPtr& req);

        /// @brief 是否免认证路径
        bool isExcludedPath(const std::string& path);
    };

}  // namespace soilwatercm::middleware
