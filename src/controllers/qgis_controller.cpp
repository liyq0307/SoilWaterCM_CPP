#include "qgis_controller.h"

#include <drogon/HttpTypes.h>

namespace soilwatercm::controllers
{

    QgisController::QgisController(std::shared_ptr<services::QgisService> qgisService)
        : qgisService_(std::move(qgisService))
    {
    }

    void QgisController::publish(const drogon::HttpRequestPtr& req,
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

        services::PublishLayerParams params;
        params.layer_name = (*json)["layer_name"].asString();
        params.data_path = (*json)["data_path"].asString();

        if ((*json).isMember("style_name"))
        {
            params.style_name = (*json)["style_name"].asString();
        }

        if ((*json).isMember("title"))
        {
            params.title = (*json)["title"].asString();
        }

        bool success = qgisService_->publishLayer(params);

        Json::Value result;
        result["success"] = success;
        result["message"] = success ? "Layer published" : "Publish failed";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void QgisController::unpublish(const drogon::HttpRequestPtr& req,
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

        bool success = qgisService_->unpublishProject(projectId, periodId);

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
            resp->setBody(R"({"error": "服务未发布或已删除"})");
            callback(resp);
        }
    }

    void QgisController::wmsUrl(const drogon::HttpRequestPtr& req,
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

        auto infoOpt = qgisService_->findWmsInfo(projectId, periodId);

        if (!infoOpt)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": ".qgs 工程不存在, 请先 POST /api/v1/qgis/publish"})");
            callback(resp);
            return;
        }

        const auto& info = *infoOpt;

        Json::Value result;
        result["qgs_path"] = info.qgsPath;
        result["service"] = info.service;
        result["fallback"] = info.fallback;
        result["wms_base"] = info.wmsBase;
        result["getcapabilities_url"] = info.getCapabilitiesUrl;
        result["getmap_base"] = info.wmsBase;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void QgisController::listServices(const drogon::HttpRequestPtr& /*req*/,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        auto services = qgisService_->listServices();

        Json::Value result(Json::arrayValue);
        for (const auto& service : services)
        {
            Json::Value item;
            item["name"] = service.name;
            item["path"] = service.path;
            item["crs"] = service.crs;

            Json::Value layers(Json::arrayValue);
            for (const auto& layer : service.layers)
            {
                layers.append(layer);
            }
            item["layers"] = layers;

            result.append(item);
        }

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void QgisController::getService(const drogon::HttpRequestPtr& /*req*/,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                    const std::string& name)
    {
        auto serviceOpt = qgisService_->getServiceInfo(name);

        if (!serviceOpt)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Service not found"})");
            callback(resp);
            return;
        }

        const auto& service = *serviceOpt;

        Json::Value result;
        result["name"] = service.name;
        result["path"] = service.path;
        result["crs"] = service.crs;

        Json::Value layers(Json::arrayValue);
        for (const auto& layer : service.layers)
        {
            layers.append(layer);
        }
        result["layers"] = layers;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void QgisController::deleteService(const drogon::HttpRequestPtr& /*req*/,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                       const std::string& name)
    {
        bool success = qgisService_->deleteService(name);

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
            resp->setBody(R"({"error": "Service not found"})");
            callback(resp);
        }
    }

}  // namespace soilwatercm::controllers
