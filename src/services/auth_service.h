#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include "../dao/user_dao.h"
#include "../models/user.h"

namespace soilwatercm::services
{

    /** @brief 登录成功后签发的 JWT 及其携带的用户信息 */
    struct TokenInfo
    {
        std::string access_token;            ///< JWT 字符串
        std::string token_type = "bearer";   ///< 令牌类型
        int user_id;                         ///< 用户 id
        std::string username;                ///< 用户名
        models::UserRole role;               ///< 角色
    };

    /**
     * @brief 用户认证与授权服务
     *
     * 登录签发 JWT、校验令牌、按角色判权限, 密码用 bcrypt 哈希存储。
     * 角色分 admin / monitor / leader 三级。
     */
    class AuthService
    {
    public:
        /**
         * @brief 构造, 注入用户 DAO 与 JWT 配置
         * @param userDao 用户 DAO
         * @param jwtSecret JWT 签名密钥
         * @param jwtExpiration 令牌有效期 (秒), 默认 7 天
         * @param jwtIssuer 签发者标识
         */
        explicit AuthService(std::shared_ptr<dao::UserDao> userDao, const std::string& jwtSecret,
                             int jwtExpiration = 604800, const std::string& jwtIssuer = "soilwatercm");

        ~AuthService() = default;

        /**
         * @brief 校验用户名密码 (禁用用户也拒绝)
         * @param username 用户名
         * @param password 明文密码
         * @return 用户; 认证失败返回 nullopt
         */
        std::optional<models::User> authenticate(const std::string& username, const std::string& password);

        /**
         * @brief 登录
         *
         * 认证通过后更新最后登录时间并签发带 role 声明的令牌。
         * @param username 用户名
         * @param password 明文密码
         * @return 令牌信息; 认证失败返回 nullopt
         */
        std::optional<TokenInfo> login(const std::string& username, const std::string& password);

        /**
         * @brief 签发 HS256 JWT
         * @param userId 用户 id (写入 subject)
         * @param role 角色 (写入 role 声明)
         * @param extraClaims 额外的自定义声明
         * @return 令牌字符串 (按配置有效期过期)
         */
        std::string generateToken(int userId, models::UserRole role,
                                  const std::map<std::string, std::string>& extraClaims = {});

        /**
         * @brief 校验令牌签名/签发者/有效期
         * @param token JWT 字符串
         * @return 令牌中的用户 id; 无效返回 nullopt
         */
        std::optional<int> validateToken(const std::string& token);

        /**
         * @brief 从令牌解析用户并查库
         * @param token JWT 字符串
         * @return 用户; 令牌无效或用户不存在返回 nullopt
         */
        std::optional<models::User> getUserFromToken(const std::string& token);

        /**
         * @brief 判断用户是否拥有某权限
         * @param userId 用户 id
         * @param permission 权限名
         * @return admin 全通过, 其余按内置角色权限表
         */
        bool hasPermission(int userId, const std::string& permission);

        /**
         * @brief 判断用户是否拥有某角色
         * @param userId 用户 id
         * @param requiredRole 要求的角色
         * @return admin 视为满足任何角色
         */
        bool hasRole(int userId, models::UserRole requiredRole);

        /// @brief bcrypt 哈希密码 (cost 12)
        static std::string hashPassword(const std::string& password);

        /// @brief 校验密码与哈希是否匹配
        static bool verifyPassword(const std::string& password, const std::string& hash);

        /**
         * @brief 注册新用户
         * @param username 用户名 (须唯一)
         * @param password 明文密码 (入库前 bcrypt 哈希)
         * @param realName 真实姓名
         * @param email 邮箱
         * @param role 角色, 默认 monitor
         * @param phone 手机号, 可空
         * @return 含新 id 的用户记录
         * @throw ValidationException 用户名已存在时
         */
        models::User registerUser(const std::string& username, const std::string& password, const std::string& realName,
                                  const std::string& email, models::UserRole role = models::UserRole::Monitor,
                                  const std::optional<std::string>& phone = std::nullopt);

        /**
         * @brief 记录最后登录时间
         * @param userId 用户 id
         * @return 更新成功返回 true
         */
        bool updateLastLogin(int userId);

    private:
        std::shared_ptr<dao::UserDao> userDao_;
        std::string jwtSecret_;
        int jwtExpiration_;
        std::string jwtIssuer_;

        std::string roleToString(models::UserRole role) const;
        models::UserRole stringToRole(const std::string& roleStr) const;
    };

}  // namespace soilwatercm::services
