#pragma once

#include <drogon/HttpController.h>

#include "../services/auth_service.h"

namespace soilwatercm::controllers
{

    /**
     * @brief 认证 HTTP 控制器
     *
     * 提供登录/登出/注册与当前用户查询, 路由前缀 /api/v1/auth。
     */
    class AuthController : public drogon::HttpController<AuthController, false>
    {
    public:
        AuthController() = default;

        /**
         * @brief 构造, 注入认证服务
         * @param authService 认证服务
         */
        explicit AuthController(std::shared_ptr<services::AuthService> authService);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(AuthController::login, "/api/v1/auth/login", drogon::Post);
        ADD_METHOD_TO(AuthController::logout, "/api/v1/auth/logout", drogon::Post);
        ADD_METHOD_TO(AuthController::getCurrentUser, "/api/v1/auth/me", drogon::Get);
        ADD_METHOD_TO(AuthController::registerUser, "/api/v1/auth/register", drogon::Post);
        METHOD_LIST_END

        /**
         * @brief POST /api/v1/auth/login 用户名密码登录
         * @return 通过 callback 返回 JWT token 与用户信息
         */
        void login(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/auth/logout 登出
         * @return 通过 callback 返回成功; 服务端无状态
         */
        void logout(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/auth/me 当前用户信息
         * @return 通过 callback 返回用户信息; 从 Bearer token 解析
         */
        void getCurrentUser(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/auth/register 注册新用户
         * @return 通过 callback 返回注册结果
         */
        void registerUser(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    private:
        std::shared_ptr<services::AuthService> authService_;
    };

}  // namespace soilwatercm::controllers
