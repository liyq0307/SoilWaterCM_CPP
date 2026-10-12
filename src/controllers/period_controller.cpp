#include "period_controller.h"

#include <drogon/HttpTypes.h>

#include "../common/exceptions.h"
#include "../utils/logger.h"
#include <nlohmann/json.hpp>

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

        std::optional<int> parseOptIntParam(const drogon::HttpRequestPtr& req, const std::string& key)
        {
            std::string v = std::string(req->getParameter(key));
            if (v.empty())
                return std::nullopt;
            try
            {
                return std::stoi(v);
            }
            catch (const std::exception&)
            {
                return std::nullopt;
            }
        }

        nlohmann::json periodToJson(const models::MonitoringPeriod& p)
        {
            nlohmann::json j;
            j["id"] = p.id;
            j["period_code"] = p.period_code;
            j["period_type"] = services::periodTypeToString(p.type);
            j["start_date"] = services::PeriodService::formatDateTime(p.start_date);
            j["end_date"] = services::PeriodService::formatDateTime(p.end_date);
            j["status"] = services::projectStatusToString(p.status);
            j["project_id"] = p.project_id;
            return j;
        }
    }  // namespace

    PeriodController::PeriodController(std::shared_ptr<services::PeriodService> periodService)
        : periodService_(std::move(periodService))
    {
    }

    void PeriodController::listPeriods(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            auto projectId = parseOptIntParam(req, "project_id");
            auto rows = periodService_->list(projectId);
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& p : rows)
                arr.push_back(periodToJson(p));
            callback(jsonResp(arr));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void PeriodController::createPeriod(const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            auto jsonOpt = req->getJsonObject();
            if (!jsonOpt)
            {
                callback(errorResp("Invalid JSON"));
                return;
            }
            nlohmann::json body = nlohmann::json::parse(req->body());

            models::MonitoringPeriod p;
            p.project_id = body.value("project_id", 0);
            p.period_code = body.value("period_code", "");

            std::string typeStr = body.value("period_type", std::string("monthly"));
            auto type = services::parsePeriodType(typeStr);
            if (!type)
            {
                callback(errorResp("无效监测期类型: " + typeStr));
                return;
            }
            p.type = *type;

            if (!body.contains("start_date") || !body.contains("end_date"))
            {
                callback(errorResp("缺少 start_date 或 end_date"));
                return;
            }
            p.start_date = services::PeriodService::parseDateTime(body["start_date"].get<std::string>());
            p.end_date = services::PeriodService::parseDateTime(body["end_date"].get<std::string>());

            if (body.contains("status") && body["status"].is_string())
            {
                auto status = services::parseProjectStatus(body["status"].get<std::string>());
                if (!status)
                {
                    callback(errorResp("无效监测期状态: " + body["status"].get<std::string>()));
                    return;
                }
                p.status = *status;
            }

            auto created = periodService_->create(p);
            callback(jsonResp(periodToJson(created), drogon::k201Created));
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

    void PeriodController::getPeriod(const drogon::HttpRequestPtr& /*req*/,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        try
        {
            auto periodOpt = periodService_->getById(id);
            if (!periodOpt)
            {
                callback(errorResp("监测期不存在", drogon::k404NotFound));
                return;
            }
            callback(jsonResp(periodToJson(*periodOpt)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void PeriodController::updateStatus(const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        try
        {
            nlohmann::json body = nlohmann::json::parse(req->body());
            if (!body.contains("status") || !body["status"].is_string())
            {
                callback(errorResp("缺少 status 字段"));
                return;
            }

            auto updated = periodService_->updateStatus(id, body["status"].get<std::string>());
            if (!updated)
            {
                callback(errorResp("监测期不存在", drogon::k404NotFound));
                return;
            }

            nlohmann::json result;
            result["id"] = updated->id;
            result["status"] = services::projectStatusToString(updated->status);
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

    void PeriodController::listArchives(const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            auto projectId = parseOptIntParam(req, "project_id");
            callback(jsonResp(periodService_->listArchives(projectId)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void PeriodController::archivePeriod(const drogon::HttpRequestPtr& /*req*/,
                                         std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        try
        {
            auto manifest = periodService_->archivePeriod(id);
            callback(jsonResp(manifest));
        }
        catch (const common::NotFoundException& e)
        {
            callback(errorResp(e.what(), drogon::k404NotFound));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void PeriodController::getArchiveManifest(const drogon::HttpRequestPtr& /*req*/,
                                              std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        try
        {
            auto manifest = periodService_->readManifest(id);
            if (!manifest)
            {
                callback(errorResp("该监测期尚未归档", drogon::k404NotFound));
                return;
            }
            callback(jsonResp(*manifest));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

}  // namespace soilwatercm::controllers
