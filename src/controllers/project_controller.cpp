#include "project_controller.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include <drogon/HttpTypes.h>
#include <drogon/MultiPart.h>
#include <nlohmann/json.hpp>

#include "../utils/geojson_utils.h"

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

        nlohmann::json documentToJson(const models::ProjectDocument& doc)
        {
            nlohmann::json j;
            j["id"] = doc.id;
            j["project_id"] = doc.project_id;
            j["doc_type"] = doc.doc_type;
            if (doc.title)
                j["title"] = *doc.title;
            else
                j["title"] = nullptr;
            j["name"] = doc.name;
            if (doc.file_size_kb)
                j["file_size_kb"] = *doc.file_size_kb;
            else
                j["file_size_kb"] = nullptr;

            auto time = std::chrono::system_clock::to_time_t(doc.uploaded_at);
            std::tm tm = {};
            localtime_s(&tm, &time);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
            j["uploaded_at"] = buf;
            return j;
        }
    }  // namespace

    ProjectController::ProjectController(std::shared_ptr<services::ProjectService> projectService)
        : projectService_(std::move(projectService))
    {
    }

    void ProjectController::listProjects(const drogon::HttpRequestPtr& /*req*/,
                                         std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        auto projects = projectService_->listProjects({});

        Json::Value result(Json::arrayValue);
        for (const auto& project : projects)
        {
            Json::Value item;
            item["id"] = project.id;
            item["code"] = project.code;
            item["name"] = project.name;
            item["status"] = static_cast<int>(project.status);
            result.append(item);
        }

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void ProjectController::createProject(const drogon::HttpRequestPtr& req,
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

        services::ProjectCreateParams params;
        params.code = (*json)["code"].asString();
        params.name = (*json)["name"].asString();

        try
        {
            auto project = projectService_->createProject(params);

            Json::Value result;
            result["id"] = project.id;
            result["code"] = project.code;
            result["name"] = project.name;

            auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
            resp->setStatusCode(drogon::k201Created);
            callback(resp);
        }
        catch (const std::exception& e)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(std::string(R"({"error": ")") + e.what() + R"("})");
            callback(resp);
        }
    }

    void ProjectController::getProject(const drogon::HttpRequestPtr& /*req*/,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        auto projectOpt = projectService_->getProjectById(id);

        if (!projectOpt)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Project not found"})");
            callback(resp);
            return;
        }

        const auto& project = *projectOpt;

        Json::Value result;
        result["id"] = project.id;
        result["code"] = project.code;
        result["name"] = project.name;
        result["status"] = static_cast<int>(project.status);

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void ProjectController::updateProject(const drogon::HttpRequestPtr& req,
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

        services::ProjectUpdateParams params;
        if ((*json).isMember("name"))
        {
            params.name = (*json)["name"].asString();
        }

        bool success = projectService_->updateProject(id, params);

        if (success)
        {
            auto projectOpt = projectService_->getProjectById(id);
            Json::Value result;
            result["id"] = projectOpt->id;
            result["code"] = projectOpt->code;
            result["name"] = projectOpt->name;

            auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
            resp->setStatusCode(drogon::k200OK);
            callback(resp);
        }
        else
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Update failed"})");
            callback(resp);
        }
    }

    void ProjectController::deleteProject(const drogon::HttpRequestPtr& /*req*/,
                                          std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        try
        {
            bool success = projectService_->deleteProject(id);

            if (success)
            {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k204NoContent);
                callback(resp);
            }
            else
            {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k400BadRequest);
                resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                resp->setBody(R"({"error": "Delete failed"})");
                callback(resp);
            }
        }
        catch (const std::exception& e)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(std::string(R"({"error": ")") + e.what() + R"("})");
            callback(resp);
        }
    }

    void ProjectController::getPeriods(const drogon::HttpRequestPtr& /*req*/,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        auto periods = projectService_->getMonitoringPeriods(id);

        Json::Value result(Json::arrayValue);
        for (const auto& period : periods)
        {
            Json::Value item;
            item["id"] = period.id;
            item["period_code"] = period.period_code;
            result.append(item);
        }

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void ProjectController::getBoundary(const drogon::HttpRequestPtr& /*req*/,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        try
        {
            auto boundaryOpt = projectService_->getBoundary(id);
            if (!boundaryOpt)
            {
                callback(errorResp("项目或边界不存在", drogon::k404NotFound));
                return;
            }

            auto geometry = utils::GeoJsonUtils::wktToGeoJson(boundaryOpt->wkt);
            if (geometry.is_null())
            {
                callback(errorResp("边界几何无法转换为 GeoJSON", drogon::k500InternalServerError));
                return;
            }
            callback(jsonResp(geometry));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void ProjectController::downloadBoundary(const drogon::HttpRequestPtr& /*req*/,
                                             std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        try
        {
            auto projectOpt = projectService_->getProjectById(id);
            auto boundaryOpt = projectService_->getBoundary(id);
            if (!projectOpt || !boundaryOpt)
            {
                callback(errorResp("项目或边界不存在", drogon::k404NotFound));
                return;
            }

            auto geometry = utils::GeoJsonUtils::wktToGeoJson(boundaryOpt->wkt);
            if (geometry.is_null())
            {
                callback(errorResp("边界几何无法转换为 GeoJSON", drogon::k500InternalServerError));
                return;
            }

            nlohmann::json properties;
            properties["project_id"] = id;
            properties["project_code"] = projectOpt->code;
            properties["project_name"] = projectOpt->name;

            nlohmann::json feature = utils::GeoJsonUtils::makeFeature(geometry, properties);
            nlohmann::json features = nlohmann::json::array({feature});

            nlohmann::json fc = utils::GeoJsonUtils::makeFeatureCollection(features);
            fc["name"] = "project_" + std::to_string(id) + "_boundary";
            nlohmann::json crs;
            crs["type"] = "name";
            crs["properties"] = nlohmann::json{{"name", "EPSG:4326"}};
            fc["crs"] = crs;

            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k200OK);
            resp->setContentTypeString("application/geo+json");
            resp->addHeader("Content-Disposition", "attachment; filename=\"project_" + std::to_string(id) +
                                                       "_boundary.geojson\"");
            resp->setBody(fc.dump(2));
            callback(resp);
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void ProjectController::uploadBoundary(const drogon::HttpRequestPtr& req,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        try
        {
            drogon::MultiPartParser parser;
            if (parser.parse(req) != 0)
            {
                callback(errorResp("未收到文件"));
                return;
            }

            std::optional<int> epsg;
            std::string epsgHeader = std::string(req->getHeader("X-EPSG"));
            if (!epsgHeader.empty())
            {
                try
                {
                    epsg = std::stoi(epsgHeader);
                }
                catch (const std::exception&)
                {
                }
            }

            auto files = parser.getFiles();
            if (files.empty())
            {
                callback(errorResp("未收到文件"));
                return;
            }

            double areaHa = -1.0;

            // 多文件 (shp 配套 .shx/.dbf/.prj): 全部落盘同一临时目录, 按 .shp 读取
            if (files.size() > 1)
            {
                std::string dir = "uploads/boundaries/" + std::to_string(id) + "_" +
                                  std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                     std::chrono::system_clock::now().time_since_epoch())
                                                     .count());
                std::filesystem::create_directories(dir);
                std::string shpPath;
                for (const auto& file : files)
                {
                    std::string name = std::filesystem::path(file.getFileName()).filename().string();
                    std::filesystem::path save = std::filesystem::path(dir) / name;
                    std::ofstream out(save, std::ios::binary);
                    out.write(file.fileData(), static_cast<std::streamsize>(file.fileLength()));
                    out.close();
                    std::string ext = std::filesystem::path(name).extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(),
                                   [](unsigned char c) { return std::tolower(c); });
                    if (ext == ".shp")
                        shpPath = save.string();
                }

                if (shpPath.empty())
                {
                    std::error_code ec;
                    std::filesystem::remove_all(dir, ec);
                    callback(errorResp("多文件上传中未找到 .shp 主文件"));
                    return;
                }

                try
                {
                    areaHa = projectService_->uploadBoundaryFromPath(id, shpPath, epsg);
                }
                catch (...)
                {
                    std::error_code ec;
                    std::filesystem::remove_all(dir, ec);
                    throw;
                }
                std::error_code ec;
                std::filesystem::remove_all(dir, ec);
            }
            else
            {
                const auto& file = files[0];
                std::string fileName = file.getFileName();
                std::string content(file.fileData(), file.fileLength());
                areaHa = projectService_->uploadBoundaryContent(id, fileName, content, epsg);
            }

            nlohmann::json result;
            result["message"] = "防治责任范围已上传";
            if (areaHa >= 0)
                result["area_ha"] = areaHa;
            else
                result["area_ha"] = nullptr;
            callback(jsonResp(result));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void ProjectController::listDocuments(const drogon::HttpRequestPtr& /*req*/,
                                          std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        try
        {
            auto docs = projectService_->listDocuments(id);
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& doc : docs)
                arr.push_back(documentToJson(doc));
            callback(jsonResp(arr));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void ProjectController::uploadDocument(const drogon::HttpRequestPtr& req,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        try
        {
            drogon::MultiPartParser parser;
            if (parser.parse(req) != 0)
            {
                callback(errorResp("解析 multipart 数据失败"));
                return;
            }

            auto files = parser.getFiles();
            if (files.empty())
            {
                callback(errorResp("未收到文件"));
                return;
            }

            std::string docType = parser.getParameter<std::string>("doc_type");
            if (docType.empty())
                docType = "other";
            std::string title = parser.getParameter<std::string>("title");

            const auto& file = files[0];
            std::string fileName = file.getFileName();
            std::string content(file.fileData(), file.fileLength());

            auto doc =
                projectService_->uploadDocument(id, docType,
                                                title.empty() ? std::nullopt : std::optional<std::string>(title),
                                                fileName, content);

            callback(jsonResp(documentToJson(doc), drogon::k201Created));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void ProjectController::downloadDocument(const drogon::HttpRequestPtr& /*req*/,
                                             std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id,
                                             int docId)
    {
        try
        {
            auto docOpt = projectService_->getDocument(id, docId);
            if (!docOpt)
            {
                callback(errorResp("资料不存在", drogon::k404NotFound));
                return;
            }
            if (docOpt->file_path.empty() || !std::filesystem::exists(docOpt->file_path))
            {
                callback(errorResp("文件已丢失", drogon::k404NotFound));
                return;
            }

            auto resp = drogon::HttpResponse::newFileResponse(docOpt->file_path, docOpt->name);
            resp->setStatusCode(drogon::k200OK);
            callback(resp);
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void ProjectController::deleteDocument(const drogon::HttpRequestPtr& /*req*/,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id,
                                           int docId)
    {
        try
        {
            bool success = projectService_->deleteDocument(id, docId);
            if (!success)
            {
                callback(errorResp("资料不存在", drogon::k404NotFound));
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
