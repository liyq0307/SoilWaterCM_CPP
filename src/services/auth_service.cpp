#include "auth_service.h"

#include <chrono>

#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>

#include "../common/exceptions.h"
#include "../utils/crypto.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{

    AuthService::AuthService(std::shared_ptr<dao::UserDao> userDao, const std::string& jwtSecret, int jwtExpiration,
                             const std::string& jwtIssuer)
        : userDao_(std::move(userDao)), jwtSecret_(jwtSecret), jwtExpiration_(jwtExpiration), jwtIssuer_(jwtIssuer)
    {
    }

    std::optional<models::User> AuthService::authenticate(const std::string& username, const std::string& password)
    {
        auto userOpt = userDao_->findByUsername(username);
        if (!userOpt)
        {
            return std::nullopt;
        }

        const auto& user = *userOpt;

        if (!user.is_active)
        {
            return std::nullopt;
        }

        if (!verifyPassword(password, user.password_hash))
        {
            return std::nullopt;
        }

        return user;
    }

    std::optional<TokenInfo> AuthService::login(const std::string& username, const std::string& password)
    {
        auto userOpt = authenticate(username, password);
        if (!userOpt)
        {
            return std::nullopt;
        }

        const auto& user = *userOpt;

        updateLastLogin(user.id);

        std::map<std::string, std::string> extraClaims;
        extraClaims["role"] = roleToString(user.role);

        std::string token = generateToken(user.id, user.role, extraClaims);

        TokenInfo tokenInfo;
        tokenInfo.access_token = token;
        tokenInfo.user_id = user.id;
        tokenInfo.username = user.username;
        tokenInfo.role = user.role;

        return tokenInfo;
    }

    std::string AuthService::generateToken(int userId, models::UserRole role,
                                           const std::map<std::string, std::string>& extraClaims)
    {
        auto now = std::chrono::system_clock::now();
        auto exp = now + std::chrono::seconds(jwtExpiration_);

        auto token = jwt::create()
                         .set_issuer(jwtIssuer_)
                         .set_subject(std::to_string(userId))
                         .set_issued_at(now)
                         .set_expires_at(exp)
                         .set_payload_claim("role", jwt::claim(roleToString(role)));

        for (const auto& [key, value] : extraClaims)
        {
            token.set_payload_claim(key, jwt::claim(value));
        }

        return token.sign(jwt::algorithm::hs256{jwtSecret_});
    }

    std::optional<int> AuthService::validateToken(const std::string& token)
    {
        try
        {
            auto decoded = jwt::decode(token);

            auto verifier = jwt::verify().allow_algorithm(jwt::algorithm::hs256{jwtSecret_}).with_issuer(jwtIssuer_);

            verifier.verify(decoded);

            auto subject = decoded.get_subject();
            return std::stoi(subject);
        }
        catch (const std::exception& e)
        {
            utils::Logger::warn("Token validation failed: {}", e.what());
            return std::nullopt;
        }
    }

    std::optional<models::User> AuthService::getUserFromToken(const std::string& token)
    {
        auto userIdOpt = validateToken(token);
        if (!userIdOpt)
        {
            return std::nullopt;
        }

        return userDao_->findById(*userIdOpt);
    }

    bool AuthService::hasPermission(int userId, const std::string& permission)
    {
        auto userOpt = userDao_->findById(userId);
        if (!userOpt)
        {
            return false;
        }

        const auto& user = *userOpt;

        if (user.role == models::UserRole::Admin)
        {
            return true;
        }

        static const std::map<models::UserRole, std::vector<std::string>> rolePermissions = {
            {models::UserRole::Admin, {"all"}},
            {models::UserRole::Monitor, {"read", "write", "process", "extract", "evaluate"}},
            {models::UserRole::Leader, {"read", "evaluate", "report"}}};

        auto it = rolePermissions.find(user.role);
        if (it == rolePermissions.end())
        {
            return false;
        }

        const auto& permissions = it->second;
        return std::find(permissions.begin(), permissions.end(), permission) != permissions.end() ||
               std::find(permissions.begin(), permissions.end(), "all") != permissions.end();
    }

    bool AuthService::hasRole(int userId, models::UserRole requiredRole)
    {
        auto userOpt = userDao_->findById(userId);
        if (!userOpt)
        {
            return false;
        }

        const auto& user = *userOpt;

        if (user.role == models::UserRole::Admin)
        {
            return true;
        }

        return user.role == requiredRole;
    }

    std::string AuthService::hashPassword(const std::string& password)
    {
        return utils::Crypto::bcryptHash(password, 12);
    }

    bool AuthService::verifyPassword(const std::string& password, const std::string& hash)
    {
        return utils::Crypto::bcryptVerify(password, hash);
    }

    models::User AuthService::registerUser(const std::string& username, const std::string& password,
                                           const std::string& realName, const std::string& email, models::UserRole role,
                                           const std::optional<std::string>& phone)
    {
        if (userDao_->existsByUsername(username))
        {
            throw common::ValidationException("用户名已存在: " + username);
        }

        models::User user;
        user.username = username;
        user.password_hash = hashPassword(password);
        user.real_name = realName;
        user.email = email;
        user.phone = phone;
        user.role = role;
        user.is_active = true;
        user.created_at = std::chrono::system_clock::now();

        int userId = userDao_->insert(user);
        user.id = userId;

        utils::Logger::info("User registered: {} (role: {})", username, roleToString(role));

        return user;
    }

    bool AuthService::updateLastLogin(int userId)
    {
        return userDao_->updateLastLogin(userId);
    }

    std::string AuthService::roleToString(models::UserRole role) const
    {
        switch (role)
        {
            case models::UserRole::Admin:
                return "admin";
            case models::UserRole::Monitor:
                return "monitor";
            case models::UserRole::Leader:
                return "leader";
            default:
                return "unknown";
        }
    }

    models::UserRole AuthService::stringToRole(const std::string& roleStr) const
    {
        if (roleStr == "admin")
            return models::UserRole::Admin;
        if (roleStr == "monitor")
            return models::UserRole::Monitor;
        if (roleStr == "leader")
            return models::UserRole::Leader;
        return models::UserRole::Monitor;
    }

}  // namespace soilwatercm::services
