#include "dashboard_controller.h"

#include <drogon/HttpTypes.h>

namespace soilwatercm::controllers
{

    DashboardController::DashboardController(std::shared_ptr<services::DashboardService> dashboardService)
        : dashboardService_(std::move(dashboardService))
    {
    }

    void DashboardController::getDashboard(const drogon::HttpRequestPtr& /*req*/,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        auto data = dashboardService_->getDashboardData();

        Json::Value result;
        result["total_projects"] = data.total_projects;
        result["active_projects"] = data.active_projects;
        result["total_images"] = data.total_images;
        result["pending_tasks"] = data.pending_tasks;
        result["running_tasks"] = data.running_tasks;
        result["completed_tasks"] = data.completed_tasks;
        result["failed_tasks"] = data.failed_tasks;
        result["pending_anomalies"] = data.pending_anomalies;

        Json::Value taskStats(Json::objectValue);
        for (const auto& [key, value] : data.task_statistics)
        {
            taskStats[key] = value;
        }
        result["task_statistics"] = taskStats;

        Json::Value areaStats(Json::objectValue);
        for (const auto& [key, value] : data.area_statistics)
        {
            areaStats[key] = value;
        }
        result["area_statistics"] = areaStats;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void DashboardController::getWorkbench(const drogon::HttpRequestPtr& req,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        std::string userIdStr = std::string(req->getParameter("user_id"));
        int userId = userIdStr.empty() ? 1 : std::stoi(userIdStr);

        auto data = dashboardService_->getWorkbenchData(userId);

        Json::Value result;

        Json::Value recentProjects(Json::arrayValue);
        for (const auto& project : data.recent_projects)
        {
            Json::Value item;
            for (const auto& [key, value] : project)
            {
                item[key] = value;
            }
            recentProjects.append(item);
        }
        result["recent_projects"] = recentProjects;

        Json::Value recentTasks(Json::arrayValue);
        for (const auto& task : data.recent_tasks)
        {
            Json::Value item;
            for (const auto& [key, value] : task)
            {
                item[key] = value;
            }
            recentTasks.append(item);
        }
        result["recent_tasks"] = recentTasks;

        Json::Value pendingAnomalies(Json::arrayValue);
        for (const auto& anomaly : data.pending_anomalies)
        {
            Json::Value item;
            for (const auto& [key, value] : anomaly)
            {
                item[key] = value;
            }
            pendingAnomalies.append(item);
        }
        result["pending_anomalies"] = pendingAnomalies;

        Json::Value quickStats(Json::objectValue);
        for (const auto& [key, value] : data.quick_stats)
        {
            quickStats[key] = value;
        }
        result["quick_stats"] = quickStats;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    namespace
    {
        // 项目看板新端点统一走 nlohmann::json (service 层返回), 直接序列化响应体
        drogon::HttpResponsePtr jsonBodyResp(const nlohmann::json& body,
                                              drogon::HttpStatusCode code = drogon::k200OK)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(code);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(body.dump());
            return resp;
        }

        drogon::HttpResponsePtr errorBodyResp(const std::string& msg,
                                              drogon::HttpStatusCode code = drogon::k400BadRequest)
        {
            nlohmann::json j;
            j["error"] = msg;
            return jsonBodyResp(j, code);
        }

        int parseIntParam(const drogon::HttpRequestPtr& req, const char* name, int fallback = 0)
        {
            std::string v = std::string(req->getParameter(name));
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
    }  // namespace

    void DashboardController::getSummary(const drogon::HttpRequestPtr& req,
                                         std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");
            if (projectId <= 0)
            {
                callback(errorBodyResp("缺少 project_id"));
                return;
            }
            callback(jsonBodyResp(dashboardService_->buildSummary(projectId)));
        }
        catch (const std::exception& e)
        {
            callback(errorBodyResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void DashboardController::getTrend(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");
            if (projectId <= 0)
            {
                callback(errorBodyResp("缺少 project_id"));
                return;
            }
            std::string indicator = std::string(req->getParameter("indicator_code"));
            if (indicator.empty())
                indicator = std::string(req->getParameter("indicator"));
            if (indicator.empty())
            {
                callback(errorBodyResp("缺少 indicator_code"));
                return;
            }
            callback(jsonBodyResp(dashboardService_->buildTrend(projectId, indicator)));
        }
        catch (const std::exception& e)
        {
            callback(errorBodyResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void DashboardController::getMultiTrend(const drogon::HttpRequestPtr& req,
                                            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");
            if (projectId <= 0)
            {
                callback(errorBodyResp("缺少 project_id"));
                return;
            }
            callback(jsonBodyResp(dashboardService_->buildMultiTrend(projectId)));
        }
        catch (const std::exception& e)
        {
            callback(errorBodyResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void DashboardController::getPeriodIndicators(const drogon::HttpRequestPtr& req,
                                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");
            int periodId = parseIntParam(req, "period_id");
            if (projectId <= 0 || periodId <= 0)
            {
                callback(errorBodyResp("缺少 project_id / period_id"));
                return;
            }
            callback(jsonBodyResp(dashboardService_->buildPeriodIndicators(projectId, periodId)));
        }
        catch (const std::exception& e)
        {
            callback(errorBodyResp(e.what(), drogon::k500InternalServerError));
        }
    }

}  // namespace soilwatercm::controllers
