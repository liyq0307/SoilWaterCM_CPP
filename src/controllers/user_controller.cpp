#include "user_controller.h"

#include <optional>
#include <string>

#include <drogon/HttpTypes.h>

namespace soilwatercm::controllers
{

    UserController::UserController(std::shared_ptr<dao::UserDao> userDao,
                                   std::shared_ptr<services::AuthService> authService)
        : userDao_(std::move(userDao)), authService_(std::move(authService))
    {
    }

    void UserController::getCurrentUser(const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        std::optional<models::User> userOpt;

        // 优先从 Authorization Bearer token 解析当前用户 (与 /auth/me 一致)
        auto authHeader = std::string(req->getHeader("Authorization"));
        if (!authHeader.empty() && authHeader.length() >= 7 && authHeader.substr(0, 7) == "Bearer ")
        {
            std::string token = authHeader.substr(7);
            userOpt = authService_->getUserFromToken(token);
        }

        // 回退: JwtMiddleware 已注入 user_id 参数
        if (!userOpt)
        {
            std::string userIdStr = std::string(req->getParameter("user_id"));
            if (!userIdStr.empty())
            {
                try
                {
                    userOpt = userDao_->findById(std::stoi(userIdStr));
                }
                catch (const std::exception&)
                {
                }
            }
        }

        if (!userOpt)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k401Unauthorized);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Not authenticated"})");
            callback(resp);
            return;
        }

        const auto& user = *userOpt;

        Json::Value result;
        result["id"] = user.id;
        result["username"] = user.username;
        result["real_name"] = user.real_name;
        result["email"] = user.email;
        result["phone"] = user.phone.value_or("");
        result["role"] = static_cast<int>(user.role);
        result["is_active"] = user.is_active;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void UserController::listUsers(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        std::string pageStr = std::string(req->getParameter("page"));
        std::string pageSizeStr = std::string(req->getParameter("page_size"));

        int page = pageStr.empty() ? 1 : std::stoi(pageStr);
        int pageSize = pageSizeStr.empty() ? 20 : std::stoi(pageSizeStr);

        auto users = userDao_->list(page, pageSize);

        Json::Value result(Json::arrayValue);
        for (const auto& user : users)
        {
            Json::Value item;
            item["id"] = user.id;
            item["username"] = user.username;
            item["real_name"] = user.real_name;
            item["email"] = user.email;
            item["phone"] = user.phone.value_or("");
            item["role"] = static_cast<int>(user.role);
            item["is_active"] = user.is_active;
            result.append(item);
        }

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void UserController::createUser(const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        auto json = req->getJsonObject();
        if (!json)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Invalid JSON"})");
            callback(resp);
            return;
        }

        std::string username = (*json)["username"].asString();
        std::string password = (*json)["password"].asString();
        std::string realName = (*json)["real_name"].asString();
        std::string email = (*json)["email"].asString();

        int roleInt = (*json).isMember("role") ? (*json)["role"].asInt() : 1;
        models::UserRole role = static_cast<models::UserRole>(roleInt);

        std::optional<std::string> phone;
        if ((*json).isMember("phone"))
        {
            phone = (*json)["phone"].asString();
        }

        try
        {
            auto user = authService_->registerUser(username, password, realName, email, role, phone);

            Json::Value result;
            result["id"] = user.id;
            result["username"] = user.username;
            result["real_name"] = user.real_name;
            result["email"] = user.email;
            result["role"] = static_cast<int>(user.role);

            auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
            resp->setStatusCode(drogon::k201Created);
            callback(resp);
        }
        catch (const std::exception& e)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(std::string(R"({"error": ")") + e.what() + R"("})");
            callback(resp);
        }
    }

    void UserController::getUser(const drogon::HttpRequestPtr& /*req*/,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        auto userOpt = userDao_->findById(id);

        if (!userOpt)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "User not found"})");
            callback(resp);
            return;
        }

        const auto& user = *userOpt;

        Json::Value result;
        result["id"] = user.id;
        result["username"] = user.username;
        result["real_name"] = user.real_name;
        result["email"] = user.email;
        result["phone"] = user.phone.value_or("");
        result["role"] = static_cast<int>(user.role);
        result["is_active"] = user.is_active;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void UserController::updateUser(const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        auto userOpt = userDao_->findById(id);

        if (!userOpt)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "User not found"})");
            callback(resp);
            return;
        }

        auto json = req->getJsonObject();
        if (!json)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Invalid JSON"})");
            callback(resp);
            return;
        }

        auto user = *userOpt;

        if ((*json).isMember("real_name"))
        {
            user.real_name = (*json)["real_name"].asString();
        }

        if ((*json).isMember("email"))
        {
            user.email = (*json)["email"].asString();
        }

        if ((*json).isMember("phone"))
        {
            user.phone = (*json)["phone"].asString();
        }

        if ((*json).isMember("role"))
        {
            user.role = static_cast<models::UserRole>((*json)["role"].asInt());
        }

        if ((*json).isMember("is_active"))
        {
            user.is_active = (*json)["is_active"].asBool();
        }

        bool success = userDao_->update(user);

        if (success)
        {
            Json::Value result;
            result["id"] = user.id;
            result["username"] = user.username;
            result["real_name"] = user.real_name;
            result["email"] = user.email;
            result["role"] = static_cast<int>(user.role);

            auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
            resp->setStatusCode(drogon::k200OK);
            callback(resp);
        }
        else
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Update failed"})");
            callback(resp);
        }
    }

    void UserController::deleteUser(const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        std::string currentUserIdStr = std::string(req->getParameter("user_id"));

        if (!currentUserIdStr.empty())
        {
            int currentUserId = std::stoi(currentUserIdStr);
            if (currentUserId == id)
            {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k400BadRequest);
                resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                resp->setBody(R"({"error": "Cannot delete yourself"})");
                callback(resp);
                return;
            }
        }

        bool success = userDao_->remove(id);

        if (success)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k204NoContent);
            callback(resp);
        }
        else
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "User not found"})");
            callback(resp);
        }
    }

    void UserController::changePassword(const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        auto json = req->getJsonObject();
        if (!json)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Invalid JSON"})");
            callback(resp);
            return;
        }

        std::string oldPassword = (*json)["old_password"].asString();
        std::string newPassword = (*json)["new_password"].asString();

        auto userOpt = userDao_->findById(id);

        if (!userOpt)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "User not found"})");
            callback(resp);
            return;
        }

        auto user = *userOpt;

        if (!services::AuthService::verifyPassword(oldPassword, user.password_hash))
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Invalid old password"})");
            callback(resp);
            return;
        }

        user.password_hash = services::AuthService::hashPassword(newPassword);

        bool success = userDao_->update(user);

        Json::Value result;
        result["success"] = success;
        result["message"] = success ? "Password changed" : "Change failed";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

}  // namespace soilwatercm::controllers
