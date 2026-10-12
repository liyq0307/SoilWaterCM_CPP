#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "common/enums.h"
#include <nlohmann/json.hpp>

namespace soilwatercm::models
{

    /**
     * @brief 系统用户, 对应 users 表
     *
     * 登录账号与角色。
     */
    struct User
    {
        int id = 0;
        std::string username;
        std::string password_hash;  ///< 密码散列, 不参与 JSON 序列化
        std::string real_name;
        std::string email;
        std::optional<std::string> phone;
        UserRole role = UserRole::Monitor;
        bool is_active = true;  ///< 停用后不能登录, 记录仍保留
        std::chrono::system_clock::time_point created_at;
        std::optional<std::chrono::system_clock::time_point> last_login;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(User, id, username, real_name, email, phone, role, is_active)
    };

}  // namespace soilwatercm::models
