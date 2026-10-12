#include "auth_controller.h"

#include <drogon/HttpTypes.h>

#include "../utils/json_utils.h"

namespace soilwatercm::controllers
{

    AuthController::AuthController(std::shared_ptr<services::AuthService> authService)
        : authService_(std::move(authService))
    {
    }

    void AuthController::login(const drogon::HttpRequestPtr& req,
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

        auto tokenInfo = authService_->login(username, password);

        if (!tokenInfo)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k401Unauthorized);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Invalid username or password"})");
            callback(resp);
            return;
        }

        Json::Value result;
        result["access_token"] = tokenInfo->access_token;
        result["token_type"] = tokenInfo->token_type;
        result["user_id"] = tokenInfo->user_id;
        result["username"] = tokenInfo->username;
        result["role"] = static_cast<int>(tokenInfo->role);

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void AuthController::logout(const drogon::HttpRequestPtr& /*req*/,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        Json::Value result;
        result["message"] = "Logged out successfully";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void AuthController::getCurrentUser(const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        auto authHeader = req->getHeader("Authorization");
        if (authHeader.empty() || authHeader.length() < 7 || authHeader.substr(0, 7) != "Bearer ")
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k401Unauthorized);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Not authenticated"})");
            callback(resp);
            return;
        }

        std::string token = authHeader.substr(7);
        auto userOpt = authService_->getUserFromToken(token);

        if (!userOpt)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k401Unauthorized);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Invalid or expired token"})");
            callback(resp);
            return;
        }

        const auto& user = *userOpt;
        Json::Value result;
        result["id"] = user.id;
        result["username"] = user.username;
        result["real_name"] = user.real_name;
        result["email"] = user.email;
        result["role"] = static_cast<int>(user.role);
        result["is_active"] = user.is_active;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void AuthController::registerUser(const drogon::HttpRequestPtr& req,
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

        try
        {
            auto user = authService_->registerUser(username, password, realName, email);

            Json::Value result;
            result["id"] = user.id;
            result["username"] = user.username;
            result["real_name"] = user.real_name;
            result["email"] = user.email;

            auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
            resp->setStatusCode(drogon::k201Created);
            callback(resp);
        }
        catch (const std::exception& e)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": ")" + std::string(e.what()) + R"("})");
            callback(resp);
        }
    }

}  // namespace soilwatercm::controllers
