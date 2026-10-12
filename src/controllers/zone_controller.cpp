#include "zone_controller.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include <drogon/HttpTypes.h>
#include <drogon/MultiPart.h>

#include "../common/exceptions.h"
#include "../utils/geojson_utils.h"
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

        nlohmann::json statsToJson(const models::ZoneStats& s)
        {
            nlohmann::json j;
            j["id"] = s.id;
            j["project_id"] = s.project_id;
            if (s.period_id)
                j["period_id"] = *s.period_id;
            else
                j["period_id"] = nullptr;
            j["zone"] = s.zone;
            if (s.designed_area_ha)
                j["designed_area_ha"] = *s.designed_area_ha;
            else
                j["designed_area_ha"] = nullptr;
            if (!s.designed_measures.empty())
            {
                try
                {
                    j["designed_measures"] = nlohmann::json::parse(s.designed_measures);
                }
                catch (const std::exception&)
                {
                    j["designed_measures"] = s.designed_measures;
                }
            }
            else
            {
                j["designed_measures"] = nullptr;
            }
            j["progress_status"] =
                s.progress_status.empty() ? nlohmann::json(nullptr) : nlohmann::json(s.progress_status);
            j["note"] = s.note.empty() ? nlohmann::json(nullptr) : nlohmann::json(s.note);
            return j;
        }
    }  // namespace

    ZoneController::ZoneController(std::shared_ptr<services::ZoneService> zoneService,
                                   std::shared_ptr<infrastructure::gdal::VectorProcessor> vectorProcessor)
        : zoneService_(std::move(zoneService)), vectorProcessor_(std::move(vectorProcessor))
    {
    }

    void ZoneController::listZoneStats(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");
            auto periodId = parseOptIntParam(req, "period_id");
            std::string zone = std::string(req->getParameter("zone"));

            auto rows = zoneService_->listStats(projectId, periodId, zone);
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& s : rows)
                arr.push_back(statsToJson(s));
            callback(jsonResp(arr));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void ZoneController::createZoneStats(const drogon::HttpRequestPtr& req,
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

            models::ZoneStats s;
            s.project_id = body.value("project_id", 0);
            if (body.contains("period_id") && body["period_id"].is_number())
                s.period_id = body["period_id"].get<int>();
            s.zone = body.value("zone", "");
            if (body.contains("designed_area_ha") && body["designed_area_ha"].is_number())
                s.designed_area_ha = body["designed_area_ha"].get<double>();
            if (body.contains("designed_measures"))
            {
                if (body["designed_measures"].is_string())
                    s.designed_measures = body["designed_measures"].get<std::string>();
                else if (!body["designed_measures"].is_null())
                    s.designed_measures = body["designed_measures"].dump();
            }
            s.progress_status = body.value("progress_status", "");
            s.note = body.value("note", "");

            auto created = zoneService_->createStats(s);
            callback(jsonResp(statsToJson(created), drogon::k201Created));
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

    void ZoneController::updateZoneStats(const drogon::HttpRequestPtr& req,
                                         std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        try
        {
            nlohmann::json body = nlohmann::json::parse(req->body());
            auto updated = zoneService_->updateStats(id, body);
            if (!updated)
            {
                callback(errorResp("ZoneStats 记录不存在", drogon::k404NotFound));
                return;
            }
            callback(jsonResp(statsToJson(*updated)));
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

    void ZoneController::deleteZoneStats(const drogon::HttpRequestPtr& /*req*/,
                                         std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        try
        {
            bool ok = zoneService_->deleteStats(id);
            if (!ok)
            {
                callback(errorResp("ZoneStats 记录不存在", drogon::k404NotFound));
                return;
            }
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k204NoContent);
            callback(resp);
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void ZoneController::listZoneCodes(const drogon::HttpRequestPtr& /*req*/,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        callback(jsonResp(services::ZoneService::zoneCodesJson()));
    }

    void ZoneController::listZoneBoundaries(const drogon::HttpRequestPtr& req,
                                            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");
            auto rows = zoneService_->listBoundaries(projectId);
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& b : rows)
            {
                nlohmann::json j;
                j["zone"] = b.zone;
                auto it = models::zoneCodes().find(b.zone);
                j["zone_name"] = it != models::zoneCodes().end() ? it->second : b.zone;
                if (b.area_ha)
                    j["area_ha"] = *b.area_ha;
                else
                    j["area_ha"] = nullptr;
                arr.push_back(j);
            }
            callback(jsonResp(arr));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void ZoneController::uploadZoneBoundaries(const drogon::HttpRequestPtr& req,
                                              std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");
            nlohmann::json body = nlohmann::json::parse(req->body());

            auto saved = zoneService_->saveBoundaries(projectId, body);
            auto assigned = zoneService_->assignZones(projectId, std::nullopt);
            zoneService_->initZoneStats(projectId, std::nullopt);

            nlohmann::json result;
            result["boundaries"] = saved;
            result["assigned"] = assigned;
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

    void ZoneController::uploadZoneBoundaryFile(const drogon::HttpRequestPtr& req,
                                                std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");

            drogon::MultiPartParser parser;
            if (parser.parse(req) != 0 || parser.getFiles().empty())
            {
                callback(errorResp("未收到文件"));
                return;
            }

            std::filesystem::create_directories("uploads/zones");
            std::vector<std::pair<std::string, std::map<std::string, std::string>>> features;

            for (const auto& file : parser.getFiles())
            {
                std::string fileName = file.getFileName();
                std::string ext = std::filesystem::path(fileName).extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

                std::string tmpPath = "uploads/zones/" + fileName;
                file.saveAs(tmpPath);

                std::string readPath = tmpPath;
                if (ext == ".zip")
                    readPath = "/vsizip/" + std::filesystem::absolute(tmpPath).string();

                if (ext != ".dxf" && ext != ".kml" && ext != ".kmz" && ext != ".shp" && ext != ".zip")
                {
                    std::error_code ec;
                    std::filesystem::remove(tmpPath, ec);
                    continue;
                }

                auto data = vectorProcessor_->read(readPath);
                for (const auto& f : data.features)
                {
                    if (!f.geometry.empty())
                        features.emplace_back(f.geometry, f.attributes);
                }

                std::error_code ec;
                std::filesystem::remove(tmpPath, ec);
            }

            if (features.empty())
            {
                callback(errorResp("文件中未找到有效分区几何"));
                return;
            }

            auto saved = zoneService_->saveBoundariesFromFeatures(projectId, features);
            auto assigned = zoneService_->assignZones(projectId, std::nullopt);
            zoneService_->initZoneStats(projectId, std::nullopt);

            nlohmann::json result;
            result["saved"] = saved.value("saved", nlohmann::json::array());
            result["assigned"] = assigned;
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

    void ZoneController::initZoneStats(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");
            auto periodId = parseOptIntParam(req, "period_id");
            auto result = zoneService_->initZoneStats(projectId, periodId);
            callback(jsonResp(result));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void ZoneController::assignZones(const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");
            auto periodId = parseOptIntParam(req, "period_id");
            auto result = zoneService_->assignZones(projectId, periodId);
            callback(jsonResp(result));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

}  // namespace soilwatercm::controllers
