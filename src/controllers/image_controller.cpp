#include "image_controller.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>

#include <drogon/HttpTypes.h>
#include <drogon/MultiPart.h>
#include <nlohmann/json.hpp>

#include "../infrastructure/config_manager.h"

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

        // 从 multipart 表单解析 ImageFormParams (字段缺失取默认值)
        services::ImageFormParams formParamsFromParser(drogon::MultiPartParser& parser,
                                                      const drogon::HttpRequestPtr& req)
        {
            services::ImageFormParams params;
            params.project_id = parser.getParameter<int>("project_id");
            if (params.project_id == 0)
                params.project_id = parseIntParam(req, "project_id");

            params.period_id = parser.getParameter<int>("period_id");
            if (params.period_id == 0)
                params.period_id = parseIntParam(req, "period_id");

            params.filename = parser.getParameter<std::string>("filename");
            if (params.filename.empty())
                params.filename = std::string(req->getParameter("filename"));

            params.source_type = parser.getParameter<std::string>("source_type");
            if (params.source_type.empty())
                params.source_type = std::string(req->getParameter("source_type"));

            params.sensor = parser.getParameter<std::string>("sensor");
            params.acquisition_date = parser.getParameter<std::string>("acquisition_date");
            params.file_format = parser.getParameter<std::string>("file_format");

            auto res = parser.getOptionalParameter<double>("resolution_m");
            if (res)
                params.resolution_m = *res;

            return params;
        }

        // urlencoded/query 表单回退 (Drogon getParameter 同时覆盖 query string 与 urlencoded body)
        services::ImageFormParams formParamsFromReq(const drogon::HttpRequestPtr& req)
        {
            services::ImageFormParams params;
            params.project_id = parseIntParam(req, "project_id");
            params.period_id = parseIntParam(req, "period_id");
            params.filename = std::string(req->getParameter("filename"));
            params.source_type = std::string(req->getParameter("source_type"));
            params.sensor = std::string(req->getParameter("sensor"));
            params.acquisition_date = std::string(req->getParameter("acquisition_date"));
            params.file_format = std::string(req->getParameter("file_format"));
            std::string resStr = std::string(req->getParameter("resolution_m"));
            if (!resStr.empty())
            {
                try
                {
                    params.resolution_m = std::stod(resStr);
                }
                catch (const std::exception&)
                {
                }
            }
            return params;
        }

        nlohmann::json imageUploadResponse(const models::Image& image, const std::string& message = "")
        {
            nlohmann::json j;
            j["id"] = image.id;
            j["name"] = std::filesystem::path(image.file_path).filename().string();
            j["image_code"] = image.image_code;
            j["file_path"] = image.file_path;
            j["file_size_mb"] = static_cast<double>(image.file_size) / 1024.0 / 1024.0;
            j["qc_status"] = image.quality_status == models::QualityStatus::Pending
                                 ? "pending"
                                 : (image.quality_status == models::QualityStatus::Valid ? "passed" : "failed");
            if (!message.empty())
                j["message"] = message;
            return j;
        }

        std::string qualityStatusStr(models::QualityStatus status)
        {
            switch (status)
            {
                case models::QualityStatus::Pending:
                    return "pending";
                case models::QualityStatus::Valid:
                    return "passed";
                case models::QualityStatus::Invalid:
                    return "failed";
                default:
                    return "pending";
            }
        }

        // 清洗 multipart 文件名: 去掉路径前缀, 保留相对路径中的目录结构
        std::string sanitizeMultipartName(const std::string& name)
        {
            std::string safe = name;
            std::replace(safe.begin(), safe.end(), '\\', '/');
            // 去掉盘符前缀 (如 "C:/path/file.tif" -> "/path/file.tif")
            auto colon = safe.find(':');
            auto slash = safe.find('/');
            if (colon != std::string::npos && (slash == std::string::npos || colon < slash))
                safe.erase(0, colon + 1);
            // 去掉开头的 / 或 . 片段
            while (!safe.empty() && (safe[0] == '/' || safe[0] == '.'))
            {
                if (safe.rfind("..", 0) == 0)
                    return safe.substr(2);
                safe.erase(0, 1);
            }
            return safe;
        }

        // 将 multipart 文件内容直接落盘 (避免 drogon saveAs 对相对路径拼接 uploadPath)
        bool writeHttpFile(const drogon::HttpFile& file, const std::filesystem::path& dest)
        {
            std::filesystem::create_directories(dest.parent_path());
            std::ofstream out(dest, std::ios::binary);
            if (!out)
                return false;
            out.write(file.fileData(), static_cast<std::streamsize>(file.fileLength()));
            return static_cast<bool>(out);
        }
    }  // namespace

    ImageController::ImageController(std::shared_ptr<services::ImageService> imageService)
        : imageService_(std::move(imageService))
    {
    }

    void ImageController::listImages(const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        std::string projectIdStr = std::string(req->getParameter("project_id"));

        std::vector<models::Image> images;

        if (!projectIdStr.empty())
        {
            int projectId = std::stoi(projectIdStr);
            images = imageService_->getImagesByProject(projectId);
        }

        Json::Value result(Json::arrayValue);
        for (const auto& image : images)
        {
            Json::Value item;
            item["id"] = image.id;
            item["image_code"] = image.image_code;
            item["type"] = static_cast<int>(image.type);
            item["file_path"] = image.file_path;
            result.append(item);
        }

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void ImageController::uploadImage(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        drogon::MultiPartParser parser;
        if (parser.parse(req) != 0)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Failed to parse multipart data"})");
            callback(resp);
            return;
        }

        auto files = parser.getFiles();
        if (files.empty())
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "No file uploaded"})");
            callback(resp);
            return;
        }

        const auto& file = files[0];
        std::string fileName = file.getFileName();
        std::string filePath = "uploads/" + fileName;

        std::filesystem::create_directories("uploads");
        file.saveAs(filePath);

        services::ImageUploadParams params;
        params.file_path = filePath;
        params.original_filename = fileName;
        params.project_id = 1;
        params.period_id = 1;

        try
        {
            auto image = imageService_->uploadImage(params);

            Json::Value result;
            result["id"] = image.id;
            result["image_code"] = image.image_code;
            result["file_path"] = image.file_path;

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

    void ImageController::getImage(const drogon::HttpRequestPtr& /*req*/,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        auto imageOpt = imageService_->getImageById(id);

        if (!imageOpt)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Image not found"})");
            callback(resp);
            return;
        }

        const auto& image = *imageOpt;

        Json::Value result;
        result["id"] = image.id;
        result["image_code"] = image.image_code;
        result["type"] = static_cast<int>(image.type);
        result["file_path"] = image.file_path;
        result["file_size"] = image.file_size;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void ImageController::deleteImage(const drogon::HttpRequestPtr& /*req*/,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        try
        {
            bool success = imageService_->deleteImage(id);

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

    void ImageController::downloadImage(const drogon::HttpRequestPtr& /*req*/,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        try
        {
            std::string filePath = imageService_->getImageFilePath(id);

            auto resp = drogon::HttpResponse::newFileResponse(filePath);
            resp->setStatusCode(drogon::k200OK);
            callback(resp);
        }
        catch (const std::exception& e)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(std::string(R"({"error": ")") + e.what() + R"("})");
            callback(resp);
        }
    }

    void ImageController::chunkInit(const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");
            std::string filename = std::string(req->getParameter("filename"));
            long long totalSize = 0;
            std::string sizeStr = std::string(req->getParameter("total_size"));
            if (!sizeStr.empty())
            {
                try
                {
                    totalSize = std::stoll(sizeStr);
                }
                catch (const std::exception&)
                {
                }
            }

            if (projectId <= 0 || filename.empty())
            {
                callback(errorResp("缺少 project_id 或 filename"));
                return;
            }

            auto result = imageService_->initChunkUpload(projectId, filename, totalSize);

            nlohmann::json j;
            j["upload_id"] = result.upload_id;
            j["chunk_size"] = result.chunk_size;
            callback(jsonResp(j));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void ImageController::uploadChunk(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            drogon::MultiPartParser parser;
            if (parser.parse(req) != 0)
            {
                callback(errorResp("解析 multipart 数据失败"));
                return;
            }

            std::string uploadId = parser.getParameter<std::string>("upload_id");
            int chunkIndex = parser.getParameter<int>("chunk_index");
            int projectId = parser.getParameter<int>("project_id");
            if (projectId == 0)
                projectId = parseIntParam(req, "project_id");

            auto files = parser.getFiles();
            if (uploadId.empty() || files.empty())
            {
                callback(errorResp("缺少 upload_id 或分片数据"));
                return;
            }

            const auto& file = files[0];
            std::string content(file.fileData(), file.fileLength());
            imageService_->saveChunk(projectId, uploadId, chunkIndex, content);

            nlohmann::json j;
            j["chunk_index"] = chunkIndex;
            j["size"] = static_cast<long long>(content.size());
            callback(jsonResp(j, drogon::k201Created));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void ImageController::mergeChunks(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            drogon::MultiPartParser parser;
            if (parser.parse(req) != 0)
            {
                callback(errorResp("解析 multipart 数据失败"));
                return;
            }

            std::string uploadId = parser.getParameter<std::string>("upload_id");
            if (uploadId.empty())
            {
                callback(errorResp("缺少 upload_id"));
                return;
            }

            auto params = formParamsFromParser(parser, req);
            if (params.filename.empty())
            {
                // upload_id 形如 {hex32}_{safe_name}, 回退取 safe_name
                auto pos = uploadId.find('_');
                params.filename = pos != std::string::npos ? uploadId.substr(pos + 1) : uploadId;
            }

            auto image = imageService_->mergeChunks(uploadId, params);

            // 附属文件与主文件同目录保存 (merge 请求中的文件均为 aux)
            std::filesystem::path base(infrastructure::ConfigManager::getInstance().getStorageRawDir());
            std::filesystem::path saveDir = base / ("project_" + std::to_string(params.project_id));
            for (const auto& f : parser.getFiles())
            {
                std::string auxName = sanitizeMultipartName(f.getFileName());
                if (auxName.empty())
                    continue;
                writeHttpFile(f, saveDir / auxName);
            }

            callback(jsonResp(imageUploadResponse(image), drogon::k201Created));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void ImageController::uploadFolder(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            drogon::MultiPartParser parser;
            if (parser.parse(req) != 0)
            {
                callback(errorResp("解析 multipart 数据失败"));
                return;
            }

            // 主文件: item name 为 "file"; 找不到则取第一个文件
            const drogon::HttpFile* mainFilePtr = nullptr;
            for (const auto& f : parser.getFiles())
            {
                if (f.getItemName() == "file")
                {
                    mainFilePtr = &f;
                    break;
                }
            }
            if (!mainFilePtr && !parser.getFiles().empty())
                mainFilePtr = &parser.getFiles()[0];
            if (!mainFilePtr)
            {
                callback(errorResp("未收到文件"));
                return;
            }

            auto params = formParamsFromParser(parser, req);
            const auto& mainFile = *mainFilePtr;
            std::string fileName = sanitizeMultipartName(mainFile.getFileName());
            if (fileName.empty())
            {
                callback(errorResp("文件名为空"));
                return;
            }
            if (params.filename.empty())
                params.filename = fileName;

            std::filesystem::path tmpDir = std::filesystem::path("uploads") / "images";
            std::filesystem::create_directories(tmpDir);
            std::filesystem::path tmpPath =
                tmpDir / (std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                             std::chrono::system_clock::now().time_since_epoch())
                                             .count()) +
                          "_" + std::filesystem::path(fileName).filename().string());
            writeHttpFile(mainFile, tmpPath);

            models::Image image;
            try
            {
                image = imageService_->importUploadedFile(params, tmpPath.string());
            }
            catch (...)
            {
                std::error_code ec;
                std::filesystem::remove(tmpPath, ec);
                throw;
            }
            std::error_code ec;
            std::filesystem::remove(tmpPath, ec);

            // 附属文件 (.ovr/.tfw/.aux.xml 等) 保存到同一目录
            std::filesystem::path saveDir = std::filesystem::path(image.file_path).parent_path();
            for (const auto& f : parser.getFiles())
            {
                if (&f == mainFilePtr)
                    continue;
                std::string auxName = sanitizeMultipartName(f.getFileName());
                if (auxName.empty())
                    continue;
                writeHttpFile(f, saveDir / auxName);
            }

            callback(jsonResp(imageUploadResponse(image), drogon::k201Created));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void ImageController::registerPath(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            services::ImageFormParams params;
            std::string filePath;
            drogon::MultiPartParser parser;
            if (parser.parse(req) == 0)
            {
                params = formParamsFromParser(parser, req);
                filePath = parser.getParameter<std::string>("file_path");
            }
            else
            {
                params = formParamsFromReq(req);
            }
            if (filePath.empty())
                filePath = std::string(req->getParameter("file_path"));
            if (filePath.empty())
            {
                callback(errorResp("缺少 file_path"));
                return;
            }

            auto image = imageService_->registerExistingPath(params, filePath);
            callback(jsonResp(imageUploadResponse(image, "registered"), drogon::k201Created));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void ImageController::registerFolder(const drogon::HttpRequestPtr& req,
                                         std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            services::ImageFormParams params;
            std::string folderPath;
            drogon::MultiPartParser parser;
            if (parser.parse(req) == 0)
            {
                params = formParamsFromParser(parser, req);
                folderPath = parser.getParameter<std::string>("folder_path");
            }
            else
            {
                params = formParamsFromReq(req);
            }
            if (folderPath.empty())
                folderPath = std::string(req->getParameter("folder_path"));
            if (folderPath.empty())
            {
                callback(errorResp("缺少 folder_path"));
                return;
            }

            auto [ids, skipped] = imageService_->registerFolder(params, folderPath);

            nlohmann::json j;
            j["registered"] = static_cast<int>(ids.size());
            j["skipped"] = skipped;
            j["image_ids"] = ids;
            j["message"] = "registered";
            callback(jsonResp(j, drogon::k201Created));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void ImageController::registerOblique(const drogon::HttpRequestPtr& req,
                                          std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            services::ImageFormParams params;
            std::string filePath;
            drogon::MultiPartParser parser;
            if (parser.parse(req) == 0)
            {
                params = formParamsFromParser(parser, req);
                filePath = parser.getParameter<std::string>("file_path");
            }
            else
            {
                params = formParamsFromReq(req);
            }
            if (filePath.empty())
                filePath = std::string(req->getParameter("file_path"));
            if (filePath.empty())
            {
                callback(errorResp("缺少 file_path"));
                return;
            }

            auto image = imageService_->registerOblique(params, filePath);
            callback(jsonResp(imageUploadResponse(image, "registered"), drogon::k201Created));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void ImageController::getObliqueTileset(const drogon::HttpRequestPtr& req,
                                            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");
            int periodId = parseIntParam(req, "period_id", -1);

            nlohmann::json result;
            result["available"] = false;
            result["tileset_url"] = nullptr;
            result["image_id"] = nullptr;
            result["name"] = nullptr;

            if (projectId > 0)
            {
                auto images = imageService_->getImagesByProject(projectId);

                // 简化版: 匹配 Uav 类型且 file_path 指向 tileset.json 且文件存在的最新记录
                // (C++ Image 模型无 source_type/file_format 字符串字段, 以 type + 文件名近似)
                const models::Image* picked = nullptr;
                for (const auto& img : images)
                {
                    if (img.type != models::ImageType::Uav)
                        continue;
                    std::string lower = img.file_path;
                    std::transform(lower.begin(), lower.end(), lower.begin(),
                                   [](unsigned char c) { return std::tolower(c); });
                    if (lower.find("tileset.json") == std::string::npos)
                        continue;
                    std::error_code ec;
                    if (!std::filesystem::is_regular_file(img.file_path, ec))
                        continue;

                    bool periodMatch = (periodId >= 0 && img.period_id == periodId);
                    bool generic = (periodId < 0 || img.period_id == 0);
                    if (periodMatch || generic)
                    {
                        if (!picked || img.id > picked->id)
                            picked = &img;
                    }
                }

                if (picked)
                {
                    result["available"] = true;
                    result["tileset_url"] = "/api/v1/images/" + std::to_string(picked->id) + "/download";
                    result["image_id"] = picked->id;
                    result["name"] = std::filesystem::path(picked->file_path).parent_path().filename().string();
                }
            }

            callback(jsonResp(result));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void ImageController::qualityCheck(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        (void)req;
        try
        {
            auto imageOpt = imageService_->getImageById(id);
            if (!imageOpt)
            {
                callback(errorResp("影像不存在", drogon::k404NotFound));
                return;
            }

            auto result = imageService_->performQualityCheck(id);

            nlohmann::json j;
            j["image_id"] = id;
            j["passed"] = result.is_valid;
            j["message"] = result.message;
            j["cloud_cover"] = result.cloud_cover;
            if (result.crs)
                j["crs"] = *result.crs;
            else
                j["crs"] = nullptr;
            if (result.resolution)
                j["resolution"] = *result.resolution;
            else
                j["resolution"] = nullptr;
            if (result.extent)
                j["extent_wkt"] = result.extent->wkt;
            else
                j["extent_wkt"] = nullptr;
            j["qc_status"] = qualityStatusStr(result.is_valid ? models::QualityStatus::Valid
                                                              : models::QualityStatus::Invalid);
            callback(jsonResp(j));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void ImageController::deletePreprocess(const drogon::HttpRequestPtr& /*req*/,
                                          std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        try
        {
            auto imageOpt = imageService_->getImageById(id);
            if (!imageOpt)
            {
                callback(errorResp("影像不存在", drogon::k404NotFound));
                return;
            }

            bool removed = imageService_->deletePreprocessResults(id);
            if (!removed)
            {
                callback(errorResp("该影像无预处理成果", drogon::k404NotFound));
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
