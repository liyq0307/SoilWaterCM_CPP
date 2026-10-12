#include "acquisition_controller.h"

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

        // Python 端点用 query 参数; 这里同时支持 JSON body 与 query 参数 (body 优先)
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

            const char* numericKeys[] = {"project_id", "period_id",   "min_lon", "min_lat",         "max_lon",
                                         "max_lat",    "cloud_cover", "gsd_cm",  "forward_overlap", "side_overlap"};
            const char* strKeys[] = {"source", "start_date", "end_date", "data_types", "camera"};

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

    AcquisitionController::AcquisitionController(std::shared_ptr<services::AcquisitionService> acquisitionService)
        : acquisitionService_(std::move(acquisitionService))
    {
    }

    void AcquisitionController::searchSatellite(const drogon::HttpRequestPtr& req,
                                                std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            auto result = acquisitionService_->searchSatellite(parseParams(req));
            callback(jsonResp(result));
        }
        catch (const common::ValidationException& e)
        {
            callback(errorResp(e.what()));
        }
        catch (const common::ExternalToolException& e)
        {
            callback(errorResp(e.what(), drogon::k502BadGateway));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k502BadGateway));
        }
    }

    void AcquisitionController::collectSatellite(const drogon::HttpRequestPtr& req,
                                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            auto taskJson = acquisitionService_->collectSatellite(parseParams(req));
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

    void AcquisitionController::flightPlan(const drogon::HttpRequestPtr& req,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            auto result = acquisitionService_->generateFlightPlan(parseParams(req));
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

    void AcquisitionController::backendStatus(const drogon::HttpRequestPtr& /*req*/,
                                              std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            callback(jsonResp(acquisitionService_->backendStatus()));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

}  // namespace soilwatercm::controllers
