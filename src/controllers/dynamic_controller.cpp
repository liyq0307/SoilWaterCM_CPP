#include "dynamic_controller.h"

#include <drogon/HttpTypes.h>

#include <nlohmann/json.hpp>

#include "../common/exceptions.h"
#include "../utils/logger.h"

namespace soilwatercm::controllers
{

    namespace
    {
        drogon::HttpResponsePtr jsonResp(const nlohmann::json& body, drogon::HttpStatusCode code = drogon::k200OK)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(code);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(body.dump());
            return resp;
        }

        drogon::HttpResponsePtr errorResp(const std::string& msg, drogon::HttpStatusCode code = drogon::k400BadRequest)
        {
            nlohmann::json j;
            j["error"] = msg;
            return jsonResp(j, code);
        }

        int parseIntParam(const drogon::HttpRequestPtr& req, const std::string& key, int fallback = 0)
        {
            std::string v = std::string(req->getParameter(key));
            if (v.empty())
                return fallback;
            try
            {
                return std::stoi(v);
            }
            catch (const std::exception&)
            {
                return fallback;
            }
        }

        // 合并 JSON body 与 query 参数 (body 优先), 对齐 Python 端点用 query 参数的习惯
        nlohmann::json parseParams(const drogon::HttpRequestPtr& req)
        {
            nlohmann::json body = nlohmann::json::object();
            if (req->getJsonObject())
            {
                try
                {
                    body = nlohmann::json::parse(req->body());
                }
                catch (const std::exception&)
                {
                    body = nlohmann::json::object();
                }
            }

            const char* numericKeys[] = {"project_id", "period_id", "fps"};
            const char* strKeys[] = {"layer", "code_a", "code_b", "mode", "formats", "path"};

            for (const char* k : numericKeys)
            {
                if (body.contains(k))
                    continue;
                std::string v = std::string(req->getParameter(k));
                if (v.empty())
                    continue;
                try
                {
                    size_t pos = 0;
                    double d = std::stod(v, &pos);
                    if (pos == v.size())
                        body[k] = d;
                }
                catch (const std::exception&)
                {
                }
            }
            for (const char* k : strKeys)
            {
                if (body.contains(k))
                    continue;
                std::string v = std::string(req->getParameter(k));
                if (!v.empty())
                    body[k] = v;
            }
            return body;
        }
    }  // namespace

    DynamicController::DynamicController(std::shared_ptr<services::DynamicService> dynamicService)
        : dynamicService_(std::move(dynamicService))
    {
    }

    void DynamicController::periodLayers(const drogon::HttpRequestPtr& req,
                                         std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");
            std::string layer = std::string(req->getParameter("layer"));
            if (layer.empty())
                layer = "fvc";
            callback(jsonResp(dynamicService_->periodLayers(projectId, layer)));
        }
        catch (const common::ValidationException& e)
        {
            callback(errorResp(e.what()));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void DynamicController::timeSeries(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            callback(jsonResp(dynamicService_->timeSeries(parseParams(req))));
        }
        catch (const common::ValidationException& e)
        {
            callback(errorResp(e.what()));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void DynamicController::comparison(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            callback(jsonResp(dynamicService_->comparison(parseParams(req))));
        }
        catch (const common::ValidationException& e)
        {
            callback(errorResp(e.what()));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void DynamicController::animation(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            callback(jsonResp(dynamicService_->animation(parseParams(req)), drogon::k201Created));
        }
        catch (const common::ValidationException& e)
        {
            callback(errorResp(e.what()));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void DynamicController::downloadFile(const drogon::HttpRequestPtr& req,
                                         std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            std::string filePath = dynamicService_->getFile(parseParams(req));
            auto resp = drogon::HttpResponse::newFileResponse(filePath);
            resp->setStatusCode(drogon::k200OK);
            callback(resp);
        }
        catch (const common::FileNotFoundException& e)
        {
            callback(errorResp(e.what(), drogon::k404NotFound));
        }
        catch (const common::ValidationException& e)
        {
            callback(errorResp(e.what(), drogon::k403Forbidden));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

}  // namespace soilwatercm::controllers
