#include "basemap_controller.h"

#include <filesystem>
#include <iomanip>
#include <sstream>

#include <drogon/HttpTypes.h>
#include <drogon/MultiPart.h>

#include "../common/exceptions.h"
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

        std::string timestampToIso(const std::chrono::system_clock::time_point& tp)
        {
            auto time = std::chrono::system_clock::to_time_t(tp);
            std::tm tm = {};
            localtime_s(&tm, &time);
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
            return oss.str();
        }

        std::string paramFromParser(const drogon::MultiPartParser& parser, const std::string& key)
        {
            auto& params = parser.getParameters();
            auto it = params.find(key);
            if (it == params.end())
                return "";
            return it->second;
        }

        nlohmann::json basemapToJson(const models::BasemapImage& b)
        {
            nlohmann::json j;
            j["id"] = b.id;
            j["name"] = b.name;
            j["file_path"] = b.file_path;
            j["file_size_mb"] = b.file_size_mb.has_value() ? nlohmann::json(*b.file_size_mb) : nlohmann::json(nullptr);
            j["width_px"] = b.width_px.has_value() ? nlohmann::json(*b.width_px) : nlohmann::json(nullptr);
            j["height_px"] = b.height_px.has_value() ? nlohmann::json(*b.height_px) : nlohmann::json(nullptr);
            j["crs"] = b.crs;
            j["band_count"] = b.band_count.has_value() ? nlohmann::json(*b.band_count) : nlohmann::json(nullptr);
            j["stretch_min"] = b.stretch_min.has_value() ? nlohmann::json(*b.stretch_min) : nlohmann::json(nullptr);
            j["stretch_max"] = b.stretch_max.has_value() ? nlohmann::json(*b.stretch_max) : nlohmann::json(nullptr);
            j["note"] = b.note.empty() ? nlohmann::json(nullptr) : nlohmann::json(b.note);
            j["created_at"] = timestampToIso(b.created_at);
            j["wms_layer_name"] = b.wms_layer_name.value_or("basemap_" + std::to_string(b.id));
            return j;
        }
    }  // namespace

    BasemapController::BasemapController(std::shared_ptr<services::BasemapService> basemapService)
        : basemapService_(std::move(basemapService))
    {
    }

    void BasemapController::listBasemaps(const drogon::HttpRequestPtr& req,
                                         std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");
            auto rows = basemapService_->list(projectId);
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& b : rows)
                arr.push_back(basemapToJson(b));
            callback(jsonResp(arr));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void BasemapController::uploadBasemap(const drogon::HttpRequestPtr& req,
                                          std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            drogon::MultiPartParser parser;
            if (parser.parse(req) != 0 || parser.getFiles().empty())
            {
                callback(errorResp("未收到文件"));
                return;
            }

            services::BasemapUploadParams params;
            std::string pidStr = paramFromParser(parser, "project_id");
            if (!pidStr.empty())
            {
                try
                {
                    params.project_id = std::stoi(pidStr);
                }
                catch (const std::exception&)
                {
                    params.project_id = 0;
                }
            }
            params.name = paramFromParser(parser, "name");
            params.note = paramFromParser(parser, "note");

            const drogon::HttpFile* mainFile = nullptr;
            for (const auto& file : parser.getFiles())
            {
                if (file.getItemName() == "file")
                {
                    mainFile = &file;
                    break;
                }
            }
            if (!mainFile)
                mainFile = &parser.getFiles()[0];

            std::string fileName = std::filesystem::path(mainFile->getFileName()).filename().string();
            if (fileName.empty())
            {
                callback(errorResp("文件名为空"));
                return;
            }

            std::string tmpPath = "./uploads/basemaps/" + fileName;
            if (mainFile->saveAs(tmpPath) != 0)
            {
                callback(errorResp("保存上传文件失败"));
                return;
            }
            params.temp_file_path = tmpPath;
            params.original_filename = fileName;

            // 附属文件 (.aux.xml/.ovr/.rrd 等) 直接落盘到存储目录
            for (const auto& file : parser.getFiles())
            {
                if (&file == mainFile)
                    continue;
                std::string auxName = std::filesystem::path(file.getFileName()).filename().string();
                if (auxName.empty())
                    continue;
                file.saveAs("./data/basemaps/" + auxName);
            }

            auto created = basemapService_->upload(params);
            callback(jsonResp(basemapToJson(created), drogon::k201Created));
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

    void BasemapController::deleteBasemap(const drogon::HttpRequestPtr& /*req*/,
                                          std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        try
        {
            bool ok = basemapService_->remove(id);
            if (!ok)
            {
                callback(errorResp("底图不存在", drogon::k404NotFound));
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

}  // namespace soilwatercm::controllers
