#include "monitoring_controller.h"

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

            const char* numericKeys[] = {"month", "project_id", "period_id", "rainfall_mm", "cloud_snow_ratio",
                                         "drop_threshold_m"};
            const char* strKeys[] = {"mode", "dsm_before", "dsm_after"};

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

    MonitoringController::MonitoringController(std::shared_ptr<services::MonitoringService> monitoringService)
        : monitoringService_(std::move(monitoringService))
    {
    }

    void MonitoringController::schedule(const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            auto result = monitoringService_->schedule(parseParams(req));
            callback(jsonResp(result));
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

    void MonitoringController::rapidChange(const drogon::HttpRequestPtr& req,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                           int projectId)
    {
        try
        {
            auto taskJson = monitoringService_->rapidChange(projectId, parseParams(req));
            callback(jsonResp(taskJson, drogon::k201Created));
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

}  // namespace soilwatercm::controllers
