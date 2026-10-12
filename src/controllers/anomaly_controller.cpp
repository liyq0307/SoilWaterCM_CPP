#include "anomaly_controller.h"

#include <drogon/HttpTypes.h>

namespace soilwatercm::controllers
{

    AnomalyController::AnomalyController(std::shared_ptr<services::AnomalyService> anomalyService)
        : anomalyService_(std::move(anomalyService))
    {
    }

    void AnomalyController::listAnomalies(const drogon::HttpRequestPtr& req,
                                          std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        std::string projectIdStr = std::string(req->getParameter("project_id"));

        std::vector<models::Anomaly> anomalies;

        if (!projectIdStr.empty())
        {
            int projectId = std::stoi(projectIdStr);
            anomalies = anomalyService_->getAnomaliesByProject(projectId);
        }

        Json::Value result(Json::arrayValue);
        for (const auto& anomaly : anomalies)
        {
            Json::Value item;
            item["id"] = anomaly.id;
            item["type"] = static_cast<int>(anomaly.type);
            item["severity"] = static_cast<int>(anomaly.severity);
            item["description"] = anomaly.description;
            item["status"] = static_cast<int>(anomaly.status);
            result.append(item);
        }

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void AnomalyController::detect(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        services::AnomalyDetectParams params;
        std::string projectIdStr = std::string(req->getParameter("project_id"));
        std::string periodIdStr = std::string(req->getParameter("period_id"));
        std::string typeStr = std::string(req->getParameter("type"));

        if (projectIdStr.empty())
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "project_id is required"})");
            callback(resp);
            return;
        }

        params.project_id = std::stoi(projectIdStr);
        params.period_id = periodIdStr.empty() ? 0 : std::stoi(periodIdStr);
        params.type = typeStr.empty() ? "disturbance" : typeStr;

        auto anomalies = anomalyService_->detectAnomalies(params);

        Json::Value result(Json::arrayValue);
        for (const auto& anomaly : anomalies)
        {
            Json::Value item;
            item["id"] = anomaly.id;
            item["type"] = static_cast<int>(anomaly.type);
            item["severity"] = static_cast<int>(anomaly.severity);
            item["description"] = anomaly.description;
            item["status"] = static_cast<int>(anomaly.status);
            result.append(item);
        }

        Json::Value body;
        body["count"] = static_cast<int>(anomalies.size());
        body["anomalies"] = result;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void AnomalyController::getAnomaly(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        auto anomalyOpt = anomalyService_->getAnomalyById(id);

        if (!anomalyOpt)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Anomaly not found"})");
            callback(resp);
            return;
        }

        const auto& anomaly = *anomalyOpt;

        Json::Value result;
        result["id"] = anomaly.id;
        result["type"] = static_cast<int>(anomaly.type);
        result["severity"] = static_cast<int>(anomaly.severity);
        result["description"] = anomaly.description;
        result["status"] = static_cast<int>(anomaly.status);

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void AnomalyController::updateAnomaly(const drogon::HttpRequestPtr& req,
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

        std::optional<models::DisposalStatus> status;
        std::optional<models::Severity> severity;
        std::optional<std::string> description;
        std::optional<std::string> measures;
        std::optional<std::string> assignee;

        if (json->isMember("status"))
        {
            status = static_cast<models::DisposalStatus>((*json)["status"].asInt());
        }
        if (json->isMember("severity"))
        {
            severity = static_cast<models::Severity>((*json)["severity"].asInt());
        }
        if (json->isMember("description"))
        {
            description = (*json)["description"].asString();
        }
        if (json->isMember("measures"))
        {
            measures = (*json)["measures"].asString();
        }
        if (json->isMember("assignee"))
        {
            assignee = (*json)["assignee"].asString();
        }

        bool success = anomalyService_->updateAnomaly(id, status, severity, description, measures, assignee);

        if (!success)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Anomaly not found"})");
            callback(resp);
            return;
        }

        auto anomalyOpt = anomalyService_->getAnomalyById(id);
        const auto& anomaly = *anomalyOpt;

        Json::Value result;
        result["id"] = anomaly.id;
        result["type"] = static_cast<int>(anomaly.type);
        result["severity"] = static_cast<int>(anomaly.severity);
        result["description"] = anomaly.description;
        result["status"] = static_cast<int>(anomaly.status);
        result["assignee"] = anomaly.assignee.value_or("");
        result["measures"] = anomaly.measures.value_or("");

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void AnomalyController::assign(const drogon::HttpRequestPtr& req,
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

        int assigneeId = (*json)["assignee_id"].asInt();

        bool success = anomalyService_->assignAnomaly(id, assigneeId);

        Json::Value result;
        result["success"] = success;
        result["message"] = success ? "Anomaly assigned" : "Assignment failed";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void AnomalyController::resolve(const drogon::HttpRequestPtr& req,
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

        std::string measures = (*json)["measures"].asString();

        bool success = anomalyService_->resolveAnomaly(id, measures);

        Json::Value result;
        result["success"] = success;
        result["message"] = success ? "Anomaly resolved" : "Resolution failed";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void AnomalyController::dispatch(const drogon::HttpRequestPtr& /*req*/,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        bool success = anomalyService_->dispatchAnomaly(id);

        if (!success)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Anomaly not found or already resolved"})");
            callback(resp);
            return;
        }

        Json::Value result;
        result["id"] = id;
        result["status"] = static_cast<int>(models::DisposalStatus::Assigned);
        result["message"] = "已派单处置 (对应 Python dispatched, 映射为 Assigned)";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void AnomalyController::notify(const drogon::HttpRequestPtr& /*req*/,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        bool success = anomalyService_->notifyAnomaly(id);

        if (!success)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Anomaly not found"})");
            callback(resp);
            return;
        }

        Json::Value channels(Json::objectValue);
        channels["email"] = "sent";
        channels["sms"] = "sent";
        channels["app"] = "sent";

        Json::Value result;
        result["id"] = id;
        result["status"] = "dispatched";
        result["channels"] = channels;
        result["note"] = "C++ 后端无短信/邮件网关, 仅记录日志占位";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

}  // namespace soilwatercm::controllers
