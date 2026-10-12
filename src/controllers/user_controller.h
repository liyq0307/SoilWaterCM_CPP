#pragma once

#include <drogon/HttpController.h>

#include "../dao/user_dao.h"
#include "../services/auth_service.h"

namespace soilwatercm::controllers
{

    /**
     * @brief 用户管理 HTTP 控制器
     *
     * 提供当前用户查询与用户 CRUD/改密, 路由前缀 /api/v1/users。
     */
    class UserController : public drogon::HttpController<UserController, false>
    {
    public:
        UserController() = default;

        /**
         * @brief 构造, 注入用户 DAO 与认证服务
         * @param userDao 用户数据访问对象
         * @param authService 认证服务 (解析 token)
         */
        UserController(std::shared_ptr<dao::UserDao> userDao, std::shared_ptr<services::AuthService> authService);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(UserController::getCurrentUser, "/api/v1/users/me", drogon::Get);
        ADD_METHOD_TO(UserController::listUsers, "/api/v1/users", drogon::Get);
        ADD_METHOD_TO(UserController::createUser, "/api/v1/users", drogon::Post);
        ADD_METHOD_TO(UserController::getUser, "/api/v1/users/{id}", drogon::Get);
        ADD_METHOD_TO(UserController::updateUser, "/api/v1/users/{id}", drogon::Put);
        ADD_METHOD_TO(UserController::deleteUser, "/api/v1/users/{id}", drogon::Delete);
        ADD_METHOD_TO(UserController::changePassword, "/api/v1/users/{id}/password", drogon::Post);
        METHOD_LIST_END

        /**
         * @brief GET /api/v1/users/me 当前登录用户信息
         * @return 通过 callback 返回用户信息; 优先解析 Bearer token, 回退到中间件注入的 user_id
         */
        void getCurrentUser(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/users?page=&page_size= 分页列出用户
         * @return 通过 callback 返回用户分页列表
         */
        void listUsers(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/users 创建用户
         * @return 通过 callback 返回 201 与新记录; 可指定角色与电话
         */
        void createUser(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/users/{id} 查询用户详情
         * @param id 用户 id
         * @return 通过 callback 返回用户记录
         */
        void getUser(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     int id);

        /**
         * @brief PUT /api/v1/users/{id} 更新用户信息
         * @param id 用户 id
         * @return 通过 callback 返回更新后的记录; 可改姓名/邮箱/电话/角色/启用状态
         */
        void updateUser(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief DELETE /api/v1/users/{id} 删除用户
         * @param id 用户 id
         * @return 通过 callback 返回 204; 不允许删除自己
         */
        void deleteUser(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief POST /api/v1/users/{id}/password 修改密码
         * @param id 用户 id
         * @return 通过 callback 返回修改结果; 先校验旧密码
         */
        void changePassword(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

    private:
        std::shared_ptr<dao::UserDao> userDao_;
        std::shared_ptr<services::AuthService> authService_;
    };

}  // namespace soilwatercm::controllers
