#include "report_controller.h"

#include <drogon/HttpTypes.h>

namespace soilwatercm::controllers
{

    ReportController::ReportController(std::shared_ptr<services::ReportService> reportService)
        : reportService_(std::move(reportService))
    {
    }

    void ReportController::listReports(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        std::string projectIdStr = std::string(req->getParameter("project_id"));

        std::vector<models::Report> reports;

        if (!projectIdStr.empty())
        {
            int projectId = std::stoi(projectIdStr);
            reports = reportService_->getReportsByProject(projectId);
        }

        Json::Value result(Json::arrayValue);
        for (const auto& report : reports)
        {
            Json::Value item;
            item["id"] = report.id;
            item["title"] = report.title;
            item["type"] = static_cast<int>(report.type);
            item["is_filed"] = report.is_filed;
            result.append(item);
        }

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void ReportController::generate(const drogon::HttpRequestPtr& req,
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

        services::ReportGenerateParams params;
        params.project_id = (*json)["project_id"].asInt();
        params.title = (*json)["title"].asString();

        auto report = reportService_->generateReport(params);

        Json::Value result;
        result["id"] = report.id;
        result["title"] = report.title;
        result["docx_path"] = report.docx_path;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k201Created);
        callback(resp);
    }

    void ReportController::progress(const drogon::HttpRequestPtr& /*req*/,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback, int taskId)
    {
        auto progressMap = reportService_->getGenerateProgress(taskId);

        if (progressMap.empty())
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "任务不存在或已过期"})");
            callback(resp);
            return;
        }

        Json::Value result;
        result["task_id"] = taskId;
        result["status"] = progressMap.count("status") ? progressMap["status"] : "";
        result["progress"] = progressMap.count("progress") ? progressMap["progress"] : "0";
        result["message"] = progressMap.count("message") ? progressMap["message"] : "";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void ReportController::previewData(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        std::string projectIdStr = std::string(req->getParameter("project_id"));
        std::string periodIdStr = std::string(req->getParameter("period_id"));

        if (projectIdStr.empty())
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "project_id is required"})");
            callback(resp);
            return;
        }

        int projectId = std::stoi(projectIdStr);
        int periodId = periodIdStr.empty() ? 0 : std::stoi(periodIdStr);

        auto meta = reportService_->getPreviewMeta(projectId, periodId);
        if (meta.empty())
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "项目不存在"})");
            callback(resp);
            return;
        }

        auto stats = reportService_->getPreviewData(projectId, periodId);

        // 聚合结构对齐 Python ReportData 的关键字段 (简化版)
        Json::Value result;
        result["project_id"] = projectId;
        result["project_name"] = meta.count("project_name") ? meta["project_name"] : "";
        result["project_code"] = meta.count("project_code") ? meta["project_code"] : "";
        result["period_id"] = periodId;
        result["period_code"] = meta.count("period_code") ? meta["period_code"] : "";
        result["period_range"] = meta.count("period_range") ? meta["period_range"] : "";
        result["report_type"] = std::string(req->getParameter("report_type"));
        if (result["report_type"].asString().empty())
        {
            result["report_type"] = "quarterly";
        }

        Json::Value monitoring;
        monitoring["disturbance_count"] = stats.count("disturbance_count") ? stats["disturbance_count"] : 0.0;
        monitoring["disturbance_area_m2"] = stats.count("disturbance_area_m2") ? stats["disturbance_area_m2"] : 0.0;
        monitoring["mean_fvc"] = stats.count("mean_fvc") ? stats["mean_fvc"] : 0.0;
        monitoring["mean_erosion_modulus"] =
            stats.count("mean_erosion_modulus") ? stats["mean_erosion_modulus"] : 0.0;
        monitoring["anomaly_total"] = stats.count("anomaly_total") ? stats["anomaly_total"] : 0.0;
        monitoring["anomaly_active"] = stats.count("anomaly_active") ? stats["anomaly_active"] : 0.0;
        result["monitoring"] = monitoring;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void ReportController::getReport(const drogon::HttpRequestPtr& /*req*/,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        auto reportOpt = reportService_->getReportById(id);

        if (!reportOpt)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Report not found"})");
            callback(resp);
            return;
        }

        const auto& report = *reportOpt;

        Json::Value result;
        result["id"] = report.id;
        result["title"] = report.title;
        result["type"] = static_cast<int>(report.type);
        result["docx_path"] = report.docx_path;
        result["pdf_path"] = report.pdf_path;
        result["is_filed"] = report.is_filed;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void ReportController::deleteReport(const drogon::HttpRequestPtr& /*req*/,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        bool success = reportService_->deleteReport(id);

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
            resp->setBody(R"({"error": "Report not found"})");
            callback(resp);
        }
    }

    void ReportController::getOpinion(const drogon::HttpRequestPtr& /*req*/,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        auto opinionOpt = reportService_->getOpinion(id);

        if (!opinionOpt)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Report not found"})");
            callback(resp);
            return;
        }

        Json::Value result;
        result["report_id"] = id;
        result["opinion_text"] = *opinionOpt;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void ReportController::saveOpinion(const drogon::HttpRequestPtr& req,
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

        std::string opinionText = json->isMember("opinion_text") ? (*json)["opinion_text"].asString() : "";

        bool success = reportService_->saveOpinion(id, opinionText);

        if (!success)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Report not found"})");
            callback(resp);
            return;
        }

        Json::Value result;
        result["report_id"] = id;
        result["status"] = "saved";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void ReportController::download(const drogon::HttpRequestPtr& /*req*/,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        auto reportOpt = reportService_->getReportById(id);

        if (!reportOpt)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Report not found"})");
            callback(resp);
            return;
        }

        const auto& report = *reportOpt;

        auto resp = drogon::HttpResponse::newFileResponse(report.docx_path);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void ReportController::fileReport(const drogon::HttpRequestPtr& req,
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

        std::string filingNumber = (*json)["filing_number"].asString();

        bool success = reportService_->markAsFiled(id, filingNumber);

        Json::Value result;
        result["success"] = success;
        result["message"] = success ? "Report filed" : "Filing failed";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

}  // namespace soilwatercm::controllers
