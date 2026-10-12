#include "scene_layer_controller.h"

#include <drogon/HttpTypes.h>

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
    }  // namespace

    SceneLayerController::SceneLayerController(std::shared_ptr<services::SceneLayerService> sceneLayerService)
        : sceneLayerService_(std::move(sceneLayerService))
    {
    }

    void SceneLayerController::disturbances(const drogon::HttpRequestPtr& req,
                                            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");
            auto periodId = parseOptIntParam(req, "period_id");
            callback(jsonResp(sceneLayerService_->disturbances(projectId, periodId)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void SceneLayerController::measures(const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");
            auto periodId = parseOptIntParam(req, "period_id");
            std::string category = std::string(req->getParameter("category"));
            callback(jsonResp(sceneLayerService_->measures(projectId, periodId, category)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void SceneLayerController::spoilDumps(const drogon::HttpRequestPtr& req,
                                          std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");
            auto periodId = parseOptIntParam(req, "period_id");
            callback(jsonResp(sceneLayerService_->spoilDumps(projectId, periodId)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void SceneLayerController::anomalies(const drogon::HttpRequestPtr& req,
                                         std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");
            auto periodId = parseOptIntParam(req, "period_id");
            callback(jsonResp(sceneLayerService_->anomalies(projectId, periodId)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void SceneLayerController::insarResults(const drogon::HttpRequestPtr& req,
                                            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");
            callback(jsonResp(sceneLayerService_->insarResults(projectId)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void SceneLayerController::inundationGeoJson(const drogon::HttpRequestPtr& req,
                                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");
            auto resultId = parseOptIntParam(req, "result_id");
            callback(jsonResp(sceneLayerService_->inundationGeoJson(projectId, resultId)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void SceneLayerController::viewshedGeoJson(const drogon::HttpRequestPtr& req,
                                               std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");
            auto resultId = parseOptIntParam(req, "result_id");
            callback(jsonResp(sceneLayerService_->viewshedGeoJson(projectId, resultId)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void SceneLayerController::availability(const drogon::HttpRequestPtr& req,
                                            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");
            auto periodId = parseOptIntParam(req, "period_id");
            callback(jsonResp(sceneLayerService_->availability(projectId, periodId)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

}  // namespace soilwatercm::controllers
