#include "processing_controller.h"

#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include <drogon/HttpTypes.h>

#include <nlohmann/json.hpp>

#include "../common/exceptions.h"
#include "../infrastructure/config_manager.h"
#include "../infrastructure/external/odm_client.h"
#include "../infrastructure/external_tools/external_tool_detector.h"
#include "../infrastructure/gdal/gdal_wrapper.h"
#include "../utils/logger.h"

namespace soilwatercm::controllers
{

    ProcessingController::ProcessingController(std::shared_ptr<services::PreprocessService> preprocessService,
                                               std::shared_ptr<services::InsarService> insarService)
        : preprocessService_(std::move(preprocessService)), insarService_(std::move(insarService))
    {
    }

    void ProcessingController::preprocess(const drogon::HttpRequestPtr& req,
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

        services::PreprocessParams params;

        if ((*json).isMember("image_ids"))
        {
            for (const auto& id : (*json)["image_ids"])
            {
                params.image_ids.push_back(id.asInt());
            }
        }

        if ((*json).isMember("output_dir"))
        {
            params.output_dir = (*json)["output_dir"].asString();
        }

        auto future = preprocessService_->submitPreprocessTask(params);

        Json::Value result;
        result["message"] = "Preprocess task submitted";
        result["status"] = "pending";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k202Accepted);
        callback(resp);
    }

    void ProcessingController::mosaic(const drogon::HttpRequestPtr& req,
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

        services::MosaicParams params;

        if ((*json).isMember("image_ids"))
        {
            for (const auto& id : (*json)["image_ids"])
            {
                params.image_ids.push_back(id.asInt());
            }
        }

        if ((*json).isMember("output_path"))
        {
            params.output_path = (*json)["output_path"].asString();
        }

        auto future = preprocessService_->submitMosaicTask(params);

        Json::Value result;
        result["message"] = "Mosaic task submitted";
        result["status"] = "pending";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k202Accepted);
        callback(resp);
    }

    namespace
    {
        drogon::HttpResponsePtr jsonResp(const Json::Value& body, drogon::HttpStatusCode code = drogon::k200OK)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(code);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            Json::StreamWriterBuilder wb;
            wb["indentation"] = "";
            resp->setBody(Json::writeString(wb, body));
            return resp;
        }

        drogon::HttpResponsePtr errorResp(const std::string& msg, drogon::HttpStatusCode code = drogon::k400BadRequest)
        {
            Json::Value j;
            j["error"] = msg;
            return jsonResp(j, code);
        }

        std::string tpToStr(const std::chrono::system_clock::time_point& tp)
        {
            auto t = std::chrono::system_clock::to_time_t(tp);
            std::tm tm = {};
            localtime_s(&tm, &t);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
            return buf;
        }

        // 项目 DEM 约定路径 (对齐 Python raw/project_{id}/dem.tif)
        std::string projectDemPath(int projectId)
        {
            return (std::filesystem::path(infrastructure::ConfigManager::instance().getStorageRawDir()) /
                    ("project_" + std::to_string(projectId)) / "dem.tif")
                .string();
        }

        std::string resultsSubdir(const std::string& sub, int projectId)
        {
            return (std::filesystem::path(infrastructure::ConfigManager::instance().getStorageResultsDir()) / sub /
                    ("p" + std::to_string(projectId)))
                .string();
        }
    }  // namespace

    std::shared_ptr<repository::PostgresRepository> ProcessingController::sharedRepo()
    {
        // main.cpp 未注入 DAO 时, 按全局配置惰性构建 (进程内共享一个连接池, 同 ProjectService 模式)
        if (!repo_)
        {
            static std::shared_ptr<repository::PostgresRepository> sharedRepo = []()
            {
                auto& config = infrastructure::ConfigManager::getInstance();
                repository::PostgresConfig pgCfg;
                pgCfg.host = config.getPostgresHost();
                pgCfg.port = config.getPostgresPort();
                pgCfg.database = config.getPostgresDatabase();
                pgCfg.user = config.getPostgresUser();
                pgCfg.password = config.getPostgresPassword();
                auto pool = std::make_shared<repository::PostgresConnectionPool>(pgCfg);
                return std::make_shared<repository::PostgresRepository>(pool);
            }();
            repo_ = sharedRepo;
        }
        return repo_;
    }

    std::shared_ptr<dao::TaskDao> ProcessingController::taskDao()
    {
        if (!taskDao_)
            taskDao_ = std::make_shared<dao::TaskDao>(sharedRepo());
        return taskDao_;
    }

    std::shared_ptr<dao::ImageDao> ProcessingController::imageDao()
    {
        if (!imageDao_)
            imageDao_ = std::make_shared<dao::ImageDao>(sharedRepo());
        return imageDao_;
    }

    std::shared_ptr<services::ThreeDService> ProcessingController::threeDService()
    {
        if (!threeDService_)
            threeDService_ = std::make_shared<services::ThreeDService>(
                std::make_shared<infrastructure::gdal::GdalWrapper>(),
                std::make_shared<infrastructure::external::OdmClient>());
        return threeDService_;
    }

    std::shared_ptr<dao::TerrainResultDao> ProcessingController::terrainResultDao()
    {
        if (!terrainResultDao_)
            terrainResultDao_ = std::make_shared<dao::TerrainResultDao>(sharedRepo());
        return terrainResultDao_;
    }

    std::shared_ptr<dao::LandUseResultDao> ProcessingController::landuseResultDao()
    {
        if (!landuseResultDao_)
            landuseResultDao_ = std::make_shared<dao::LandUseResultDao>(sharedRepo());
        return landuseResultDao_;
    }

    std::shared_ptr<dao::SegmentationResultDao> ProcessingController::segmentationResultDao()
    {
        if (!segmentationResultDao_)
            segmentationResultDao_ = std::make_shared<dao::SegmentationResultDao>(sharedRepo());
        return segmentationResultDao_;
    }

    std::shared_ptr<dao::SpoilDumpDao> ProcessingController::spoilDumpDao()
    {
        if (!spoilDumpDao_)
            spoilDumpDao_ = std::make_shared<dao::SpoilDumpDao>(sharedRepo());
        return spoilDumpDao_;
    }

    std::shared_ptr<dao::TrainedModelDao> ProcessingController::trainedModelDao()
    {
        if (!trainedModelDao_)
            trainedModelDao_ = std::make_shared<dao::TrainedModelDao>(sharedRepo());
        return trainedModelDao_;
    }

    Json::Value ProcessingController::param(const drogon::HttpRequestPtr& req, const std::string& key)
    {
        auto json = req->getJsonObject();
        if (json && json->isMember(key))
            return (*json)[key];
        std::string v = std::string(req->getParameter(key));
        if (v.empty())
            return Json::Value();
        try
        {
            size_t pos = 0;
            double d = std::stod(v, &pos);
            if (pos == v.size())
                return Json::Value(d);
        }
        catch (const std::exception&)
        {
        }
        return Json::Value(v);
    }

    std::string ProcessingController::strParam(const drogon::HttpRequestPtr& req, const std::string& key,
                                               const std::string& fallback)
    {
        auto v = param(req, key);
        if (v.isNull())
            return fallback;
        if (v.isString())
            return v.asString();
        return fallback;
    }

    double ProcessingController::numParam(const drogon::HttpRequestPtr& req, const std::string& key, double fallback)
    {
        auto v = param(req, key);
        if (v.isNull())
            return fallback;
        if (v.isNumeric())
            return v.asDouble();
        if (v.isString())
        {
            try
            {
                return std::stod(v.asString());
            }
            catch (const std::exception&)
            {
                return fallback;
            }
        }
        return fallback;
    }

    std::shared_ptr<services::AnomalyService> ProcessingController::anomalyService()
    {
        if (!anomalyService_)
            anomalyService_ = std::make_shared<services::AnomalyService>(std::make_shared<dao::AnomalyDao>(sharedRepo()));
        return anomalyService_;
    }

    models::Task ProcessingController::createTaskRecord(models::TaskType type, int projectId, int periodId,
                                                         const Json::Value& taskParams, const std::string& taskType)
    {
        Json::Value p = taskParams;
        p["task_type"] = taskType;

        models::Task task;
        task.type = type;
        task.status = models::TaskStatus::Pending;
        task.progress = 0.0;
        Json::StreamWriterBuilder wb;
        wb["indentation"] = "";
        task.params = Json::writeString(wb, p);
        task.project_id = projectId;
        task.period_id = periodId;
        task.retry_count = 0;
        task.submitted_at = std::chrono::system_clock::now();

        int id = taskDao()->insert(task);
        if (id < 0)
            throw common::DatabaseException("创建任务失败: " + taskType);
        task.id = id;
        return task;
    }

    Json::Value ProcessingController::taskToJson(const models::Task& task)
    {
        Json::Value j;
        j["id"] = task.id;
        j["type"] = static_cast<int>(task.type);
        j["status"] = static_cast<int>(task.status);
        j["progress"] = task.progress;
        if (!task.params.empty())
        {
            Json::CharReaderBuilder rb;
            Json::Value pv;
            std::string errs;
            std::istringstream iss(task.params);
            if (Json::parseFromStream(rb, iss, &pv, &errs))
                j["params"] = pv;
            else
                j["params"] = task.params;
        }
        else
        {
            j["params"] = Json::nullValue;
        }
        j["result_path"] = task.result_path.value_or("");
        j["error_message"] = task.error_message.value_or("");
        j["submitted_at"] = tpToStr(task.submitted_at);
        j["started_at"] = task.started_at ? Json::Value(tpToStr(*task.started_at)) : Json::Value();
        j["completed_at"] = task.completed_at ? Json::Value(tpToStr(*task.completed_at)) : Json::Value();
        j["retry_count"] = task.retry_count;
        j["project_id"] = task.project_id;
        j["period_id"] = task.period_id;
        return j;
    }

    void ProcessingController::runPlaceholderTask(int taskId, std::string outputDir, std::string taskType)
    {
        // 算法未移植: 不再"假成功"(置 Completed 却不产成果会误导前端轮询),
        // 改为显式 Failed 并写清原因, 前端可据此提示"该能力未启用"。
        try
        {
            taskDao()->updateStatus(taskId, models::TaskStatus::Running, 10.0);
            std::filesystem::create_directories(outputDir);

            Json::Value ph;
            ph["status"] = "not_implemented";
            ph["note"] = taskType + " 算法未移植, 任务未生成成果";
            std::ofstream ofs((std::filesystem::path(outputDir) / (taskType + ".json")).string());
            ofs << ph.toStyledString();
            ofs.close();

            taskDao()->updateResult(taskId, outputDir);
            taskDao()->updateStatusAndMessage(taskId, models::TaskStatus::Failed, 10.0,
                                              taskType + ": 算法未移植 (C++ 后端未实现该处理), 无成果生成");
            utils::Logger::warn("Processing: placeholder task {} ({}) marked failed (not implemented)", taskId,
                                taskType);
        }
        catch (const std::exception& e)
        {
            utils::Logger::error("Processing: placeholder task {} failed: {}", taskId, e.what());
            try
            {
                taskDao()->updateStatus(taskId, models::TaskStatus::Failed, 0.0);
            }
            catch (...)
            {
            }
        }
    }

    void ProcessingController::runInundationTask(int taskId, int projectId, std::string demPath, double waterLevel)
    {
        try
        {
            if (!std::filesystem::exists(demPath))
                throw common::FileNotFoundException("项目 DEM 不存在: " + demPath);

            taskDao()->updateStatus(taskId, models::TaskStatus::Running, 20.0);

            services::InundationParams params;
            params.dem_path = demPath;
            params.water_level = waterLevel;

            services::InundationStats stats;
            std::string out = threeDService()->analyzeInundation(params, &stats);
            taskDao()->updateResult(taskId, out);

            // 落 terrain_results (analysis=inundation), 供 scene-layers 淹没图层与 terrain/results 读取
            models::TerrainResult tr;
            tr.project_id = projectId;
            tr.analysis = "inundation";
            tr.raster_path = out;
            nlohmann::json sj;
            sj["water_level_m"] = waterLevel;
            sj["inundated_area_m2"] = stats.inundated_area_m2;
            sj["total_area_m2"] = stats.total_area_m2;
            sj["depth_mean_m"] = stats.depth_mean_m;
            sj["depth_max_m"] = stats.depth_max_m;
            sj["depth_raster"] = stats.depth_path;
            tr.stats_json = sj.dump();
            tr.created_at = std::chrono::system_clock::now();
            terrainResultDao()->insert(tr);

            taskDao()->updateStatus(taskId, models::TaskStatus::Completed, 100.0);
            utils::Logger::info("Processing: inundation task {} done, out={}", taskId, out);
        }
        catch (const std::exception& e)
        {
            utils::Logger::error("Processing: inundation task {} failed: {}", taskId, e.what());
            try
            {
                taskDao()->updateStatus(taskId, models::TaskStatus::Failed, 0.0);
            }
            catch (...)
            {
            }
        }
    }

    void ProcessingController::runViewshedTask(int taskId, int projectId, std::string demPath, double obsLon,
                                                double obsLat, double obsHeight, double radiusM)
    {
        try
        {
            if (!std::filesystem::exists(demPath))
                throw common::FileNotFoundException("项目 DEM 不存在: " + demPath);

            taskDao()->updateStatus(taskId, models::TaskStatus::Running, 20.0);

            services::ViewshedParams params;
            params.dem_path = demPath;
            params.observer_x = obsLon;
            params.observer_y = obsLat;
            params.observer_height = obsHeight;
            params.max_distance = radiusM > 0.0 ? radiusM : 2000.0;

            services::ViewshedStats stats;
            std::string out = threeDService()->analyzeViewshed(params, &stats);
            taskDao()->updateResult(taskId, out);

            // 落 terrain_results (analysis=viewshed)
            models::TerrainResult tr;
            tr.project_id = projectId;
            tr.analysis = "viewshed";
            tr.raster_path = out;
            nlohmann::json sj;
            sj["visible_area_m2"] = stats.visible_area_m2;
            sj["total_area_m2"] = stats.total_area_m2;
            sj["visible_ratio"] = stats.visible_ratio;
            tr.stats_json = sj.dump();
            tr.created_at = std::chrono::system_clock::now();
            terrainResultDao()->insert(tr);

            taskDao()->updateStatus(taskId, models::TaskStatus::Completed, 100.0);
            utils::Logger::info("Processing: viewshed task {} done, out={}", taskId, out);
        }
        catch (const std::exception& e)
        {
            utils::Logger::error("Processing: viewshed task {} failed: {}", taskId, e.what());
            try
            {
                taskDao()->updateStatus(taskId, models::TaskStatus::Failed, 0.0);
            }
            catch (...)
            {
            }
        }
    }

    void ProcessingController::preprocessById(const drogon::HttpRequestPtr& req,
                                              std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                              int imageId)
    {
        try
        {
            auto img = imageDao()->findById(imageId);
            if (!img)
            {
                callback(errorResp("影像不存在: " + std::to_string(imageId), drogon::k404NotFound));
                return;
            }

            services::PreprocessParams params;
            params.image_ids = {imageId};
            params.output_dir = (std::filesystem::path(
                                     infrastructure::ConfigManager::instance().getStorageProcessedDir()) /
                                 ("project_" + std::to_string(img->project_id)) / "preprocess")
                                    .string();

            // 对齐 Python rad_method/dst_crs/dem_path 查询参数 (auto 时跳过对应步骤)
            std::string radMethod = strParam(req, "rad_method");
            params.radiometric_correction = !radMethod.empty() && radMethod != "auto";
            std::string dstCrs = strParam(req, "dst_crs");
            if (!dstCrs.empty() && dstCrs != "EPSG:4326")
            {
                params.geometric_correction = true;
                std::string digits;
                for (char c : dstCrs)
                    if (std::isdigit(static_cast<unsigned char>(c)))
                        digits += c;
                if (!digits.empty())
                    params.target_epsg = std::stoi(digits);
            }
            std::string demPath = strParam(req, "dem_path");
            params.orthorectification = !demPath.empty();

            Json::Value taskParams;
            taskParams["image_id"] = imageId;
            taskParams["rad_method"] = radMethod.empty() ? "auto" : radMethod;
            if (!dstCrs.empty())
                taskParams["dst_crs"] = dstCrs;
            if (!demPath.empty())
                taskParams["dem_path"] = demPath;

            auto task = createTaskRecord(models::TaskType::Preprocess, img->project_id, img->period_id, taskParams,
                                         "preprocess");

            std::thread([this, params, taskId = task.id]()
                        { preprocessService_->executePreprocess(params, taskId); })
                .detach();

            callback(jsonResp(taskToJson(task)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ProcessingController::threeD(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& callback, int projectId)
    {
        (void)req;
        try
        {
            Json::Value taskParams;
            auto task = createTaskRecord(models::TaskType::ThreeD, projectId, 0, taskParams, "three_d_build");

            std::thread(
                [this, taskId = task.id, projectId]()
                {
                    std::string outDir = resultsSubdir("three_d", projectId);
                    try
                    {
                        taskDao()->updateStatus(taskId, models::TaskStatus::Running, 10.0);
                        std::filesystem::create_directories(outDir);

                        // 取项目最新 DEM/DSM 影像作为输入 (Python 走 ODM 重建, C++ 简化为 3D Tiles 转换)
                        std::string input;
                        int bestId = -1;
                        for (const auto& img : imageDao()->findByProjectId(projectId))
                        {
                            if (img.type == models::ImageType::Dem && !img.file_path.empty() && img.id > bestId)
                            {
                                bestId = img.id;
                                input = img.file_path;
                            }
                        }

                        if (input.empty())
                        {
                            Json::Value ph;
                            ph["status"] = "no_input";
                            ph["note"] = "项目未登记 DEM/DSM 影像, 无法三维建模 (ODM 重建链路未移植)";
                            std::ofstream ofs((std::filesystem::path(outDir) / "three_d_build.json").string());
                            ofs << ph.toStyledString();
                            ofs.close();
                        }
                        else
                        {
                            services::ThreeDTilesParams p;
                            p.input_path = input;
                            p.output_path = outDir;
                            threeDService()->convertTo3DTiles(p);
                        }

                        taskDao()->updateResult(taskId, outDir);
                        taskDao()->updateStatus(taskId, models::TaskStatus::Completed, 100.0);
                    }
                    catch (const std::exception& e)
                    {
                        utils::Logger::error("Processing: three_d task {} failed: {}", taskId, e.what());
                        try
                        {
                            taskDao()->updateStatus(taskId, models::TaskStatus::Failed, 0.0);
                        }
                        catch (...)
                        {
                        }
                    }
                })
                .detach();

            callback(jsonResp(taskToJson(task)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ProcessingController::insar(const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback, int projectId)
    {
        try
        {
            int periodId = static_cast<int>(numParam(req, "period_id"));
            if (periodId <= 0)
            {
                callback(errorResp("缺少 period_id"));
                return;
            }

            // 无注入的 InSAR 处理链时降级为占位 (保持旧行为, 任务登记后置失败)
            if (!insarService_)
            {
                Json::Value taskParams;
                taskParams["period_id"] = periodId;
                auto task = createTaskRecord(models::TaskType::Insar, projectId, periodId, taskParams, "insar");
                std::thread([this, taskId = task.id]()
                             {
                                 try
                                 {
                                     taskDao()->updateStatusAndMessage(
                                         taskId, models::TaskStatus::Failed, 0.0, "InSAR 后端依赖未注入");
                                 }
                                 catch (...)
                                 {
                                 }
                             })
                             .detach();
                callback(jsonResp(taskToJson(task)));
                return;
            }

            Json::Value taskParams;
            taskParams["period_id"] = periodId;
            auto task = createTaskRecord(models::TaskType::Insar, projectId, periodId, taskParams, "insar");

            // 取该项目最近 2 期 SAR 影像, 较老为主, 较新为从 (对齐 Python task_insar_process)
            auto sars = imageDao()->findLatestSar(projectId, 2);
            if (sars.size() < 2)
            {
                taskDao()->updateStatusAndMessage(
                    task.id, models::TaskStatus::Failed, 0.0, "InSAR 需要至少 2 期 SAR 影像");
                callback(jsonResp(taskToJson(task)));
                return;
            }

            services::InsarProcessParams params;
            params.project_id = projectId;
            params.period_id = periodId;
            params.master_slc_path = sars[1].file_path;
            params.slave_slc_path = sars[0].file_path;
            params.output_dir = resultsSubdir("insar", projectId);
            params.process_type = strParam(req, "process_type", "dinsar");

            auto insarFut = insarService_->submitInsarTask(params, task.id);
            // 后台等结果: 成功则落 terrain_results (analysis=insar) 并做形变异常检测 (对齐 Python task_insar_process)
            auto futPtr = std::make_shared<std::future<services::InsarProcessResult>>(std::move(insarFut));
            std::thread([this, projectId, periodId, taskId = task.id, futPtr]()
                         {
                             try
                             {
                                 auto result = futPtr->get();
                                 if (result.success && result.deformation_path)
                                 {
                                     models::TerrainResult tr;
                                     tr.project_id = projectId;
                                     tr.analysis = "insar";
                                     tr.raster_path = *result.deformation_path;
                                     tr.stats_json = R"({"method":"insar"})";
                                     tr.created_at = std::chrono::system_clock::now();
                                     terrainResultDao()->insert(tr);

                                     auto anomalies = anomalyService()->detectInsarDeformation(
                                         projectId, periodId, *result.deformation_path);
                                     utils::Logger::info("InSAR task {} done, anomalies={}", taskId,
                                                          anomalies.size());
                                 }
                             }
                             catch (const std::exception& e)
                             {
                                 utils::Logger::error("InSAR task {} post-process failed: {}", taskId, e.what());
                                 try
                                 {
                                     taskDao()->updateStatus(taskId, models::TaskStatus::Failed, 0.0);
                                 }
                                 catch (...)
                                 {
                                 }
                             }
                         })
                         .detach();

            callback(jsonResp(taskToJson(task)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ProcessingController::changeDetection(const drogon::HttpRequestPtr& req,
                                               std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                               int projectId)
    {
        try
        {
            int beforeId = static_cast<int>(numParam(req, "before_image_id"));
            int afterId = static_cast<int>(numParam(req, "after_image_id"));
            if (beforeId <= 0 || afterId <= 0)
            {
                callback(errorResp("缺少 before_image_id / after_image_id"));
                return;
            }

            Json::Value taskParams;
            taskParams["before_image_id"] = beforeId;
            taskParams["after_image_id"] = afterId;
            auto task = createTaskRecord(models::TaskType::Disturbance, projectId, 0, taskParams, "change_detection");

            std::string outDir = resultsSubdir("change_detection", projectId);
            std::thread([this, taskId = task.id, outDir]() { runPlaceholderTask(taskId, outDir, "change_detection"); })
                .detach();

            callback(jsonResp(taskToJson(task)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ProcessingController::terrain(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback, int projectId)
    {
        try
        {
            std::string analysis = strParam(req, "analysis");
            if (analysis.empty())
                analysis = "twi";

            Json::Value taskParams;
            taskParams["analysis"] = analysis;
            auto task = createTaskRecord(models::TaskType::Preprocess, projectId, 0, taskParams, "terrain_analysis");

            // SAGA/D8 地形水文分析未移植 (raster_processor 无坡度/TWI 派生能力)
            std::string outDir = resultsSubdir("terrain", projectId);
            std::thread([this, taskId = task.id, outDir]() { runPlaceholderTask(taskId, outDir, "terrain_analysis"); })
                .detach();

            callback(jsonResp(taskToJson(task)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ProcessingController::watershed(const drogon::HttpRequestPtr& req,
                                         std::function<void(const drogon::HttpResponsePtr&)>&& callback, int projectId)
    {
        try
        {
            double lon = numParam(req, "lon");
            double lat = numParam(req, "lat");
            if (lon == 0.0 || lat == 0.0)
            {
                callback(errorResp("缺少 lon / lat (卡口点坐标)"));
                return;
            }

            Json::Value taskParams;
            taskParams["lon"] = lon;
            taskParams["lat"] = lat;
            auto task = createTaskRecord(models::TaskType::Preprocess, projectId, 0, taskParams, "watershed");

            // D8 汇水区提取未移植
            std::string outDir = resultsSubdir("watershed", projectId);
            std::thread([this, taskId = task.id, outDir]() { runPlaceholderTask(taskId, outDir, "watershed"); })
                .detach();

            callback(jsonResp(taskToJson(task)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ProcessingController::inundation(const drogon::HttpRequestPtr& req,
                                          std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                          int projectId)
    {
        try
        {
            auto wlJson = param(req, "water_level_m");
            if (wlJson.isNull())
            {
                callback(errorResp("缺少 water_level_m"));
                return;
            }
            double waterLevel = numParam(req, "water_level_m");

            Json::Value taskParams;
            taskParams["water_level_m"] = waterLevel;
            auto task = createTaskRecord(models::TaskType::Preprocess, projectId, 0, taskParams, "inundation");

            // ThreeDService::analyzeInundation 已有基于 DEM 的水位淹没能力
            std::string demPath = projectDemPath(projectId);
            std::thread([this, taskId = task.id, projectId, demPath, waterLevel]()
                        { runInundationTask(taskId, projectId, demPath, waterLevel); })
                .detach();

            callback(jsonResp(taskToJson(task)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ProcessingController::viewshed(const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback, int projectId)
    {
        try
        {
            double obsLon = numParam(req, "obs_lon");
            double obsLat = numParam(req, "obs_lat");
            if (obsLon == 0.0 || obsLat == 0.0)
            {
                callback(errorResp("缺少 obs_lon / obs_lat (视点坐标)"));
                return;
            }
            double obsHeight = numParam(req, "obs_height", 2.0);
            double radiusM = numParam(req, "radius_m");

            Json::Value taskParams;
            taskParams["obs_lon"] = obsLon;
            taskParams["obs_lat"] = obsLat;
            taskParams["obs_height"] = obsHeight;
            if (radiusM > 0.0)
                taskParams["radius_m"] = radiusM;
            auto task = createTaskRecord(models::TaskType::Preprocess, projectId, 0, taskParams, "viewshed");

            // ThreeDService::analyzeViewshed 已有基于 DEM 的通视能力
            std::string demPath = projectDemPath(projectId);
            std::thread([this, taskId = task.id, projectId, demPath, obsLon, obsLat, obsHeight, radiusM]()
                        { runViewshedTask(taskId, projectId, demPath, obsLon, obsLat, obsHeight, radiusM); })
                .detach();

            callback(jsonResp(taskToJson(task)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ProcessingController::surveySlopeFarmland(const drogon::HttpRequestPtr& req,
                                                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                                   int projectId)
    {
        try
        {
            std::string landusePath = strParam(req, "landuse_path");
            if (landusePath.empty())
            {
                callback(errorResp("缺少 landuse_path (土地利用分类栅格)"));
                return;
            }

            Json::Value taskParams;
            taskParams["landuse_path"] = landusePath;
            auto task = createTaskRecord(models::TaskType::Preprocess, projectId, 0, taskParams, "slope_farmland");

            std::string outDir = resultsSubdir("survey", projectId);
            std::thread([this, taskId = task.id, outDir]() { runPlaceholderTask(taskId, outDir, "slope_farmland"); })
                .detach();

            callback(jsonResp(taskToJson(task)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ProcessingController::surveyCheckDams(const drogon::HttpRequestPtr& req,
                                               std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                               int projectId)
    {
        try
        {
            Json::Value taskParams;
            if (!param(req, "flat_deg").isNull())
                taskParams["flat_deg"] = numParam(req, "flat_deg");
            if (!param(req, "min_acc").isNull())
                taskParams["min_acc"] = numParam(req, "min_acc");
            auto task = createTaskRecord(models::TaskType::Preprocess, projectId, 0, taskParams, "check_dam_survey");

            // DEM 沟道平地 + 汇流候选区算法未移植
            std::string outDir = resultsSubdir("survey", projectId);
            std::thread([this, taskId = task.id, outDir]() { runPlaceholderTask(taskId, outDir, "check_dam_survey"); })
                .detach();

            callback(jsonResp(taskToJson(task)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ProcessingController::surfaceDiscriminate(const drogon::HttpRequestPtr& req,
                                                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                                   int projectId)
    {
        try
        {
            std::string imagePath = strParam(req, "image_path");
            if (imagePath.empty())
            {
                callback(errorResp("缺少 image_path"));
                return;
            }

            Json::Value taskParams;
            taskParams["image_path"] = imagePath;
            if (!param(req, "bands").isNull())
                taskParams["bands"] = param(req, "bands");
            auto task =
                createTaskRecord(models::TaskType::Preprocess, projectId, 0, taskParams, "surface_discriminate");

            // 规则光谱判别 (裸地/尾矿/硬化面) 未移植
            std::string outDir = resultsSubdir("surface_discriminate", projectId);
            std::thread(
                [this, taskId = task.id, outDir]() { runPlaceholderTask(taskId, outDir, "surface_discriminate"); })
                .detach();

            callback(jsonResp(taskToJson(task)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ProcessingController::thematicRaster(const drogon::HttpRequestPtr& req,
                                              std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = static_cast<int>(numParam(req, "project_id"));
            std::string rasterPath = strParam(req, "raster_path");
            if (projectId <= 0 || rasterPath.empty())
            {
                callback(errorResp("缺少 project_id / raster_path"));
                return;
            }

            Json::Value taskParams;
            taskParams["raster_path"] = rasterPath;
            std::string kind = strParam(req, "kind");
            taskParams["kind"] = kind.empty() ? "fvc" : kind;
            auto task = createTaskRecord(models::TaskType::Report, projectId, 0, taskParams, "thematic_raster");

            // PNG 专题图渲染未移植
            std::string outDir = resultsSubdir("thematic", projectId);
            std::thread([this, taskId = task.id, outDir]() { runPlaceholderTask(taskId, outDir, "thematic_raster"); })
                .detach();

            callback(jsonResp(taskToJson(task)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ProcessingController::thematicVector(const drogon::HttpRequestPtr& req,
                                              std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = static_cast<int>(numParam(req, "project_id"));
            if (projectId <= 0)
            {
                callback(errorResp("缺少 project_id"));
                return;
            }

            Json::Value taskParams;
            std::string layer = strParam(req, "layer");
            taskParams["layer"] = layer.empty() ? "disturbance" : layer;
            int periodId = static_cast<int>(numParam(req, "period_id"));
            auto task = createTaskRecord(models::TaskType::Report, projectId, periodId, taskParams, "thematic_vector");

            std::string outDir = resultsSubdir("thematic", projectId);
            std::thread([this, taskId = task.id, outDir]() { runPlaceholderTask(taskId, outDir, "thematic_vector"); })
                .detach();

            callback(jsonResp(taskToJson(task)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    namespace
    {
        Json::Value toolStatusJson(const std::map<std::string, infrastructure::ToolDetectionResult>& results,
                                   const std::vector<std::string>& keys)
        {
            Json::Value out;
            for (const auto& k : keys)
            {
                auto it = results.find(k);
                if (it == results.end())
                    continue;
                Json::Value t;
                t["available"] = it->second.available;
                t["version"] = it->second.version;
                t["path"] = it->second.path;
                out[k] = t;
            }
            return out;
        }
    }  // namespace

    void ProcessingController::insarBackendStatus(const drogon::HttpRequestPtr& /*req*/,
                                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            infrastructure::ExternalToolDetector detector;
            detector.detectAll();
            auto results = detector.getAllResults();

            Json::Value out = toolStatusJson(results, {"isce2", "snap", "snaphu", "mintpy", "pyaps3"});
            auto tool = detector.detectInsarTool();
            out["selected"] = tool == infrastructure::InsarTool::ISCE2
                                  ? "isce2"
                                  : (tool == infrastructure::InsarTool::SNAP ? "snap" : "none");
            callback(jsonResp(out));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ProcessingController::gisBackendStatus(const drogon::HttpRequestPtr& /*req*/,
                                                std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            infrastructure::ExternalToolDetector detector;
            detector.detectAll();
            auto results = detector.getAllResults();

            Json::Value out = toolStatusJson(results, {"qgis", "grass", "saga", "otb"});
            out["note"] = "gdal CLI 可用性未检测 (C++ 端 GDAL 为库链接, 非 CLI)";
            callback(jsonResp(out));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ProcessingController::terrainResults(const drogon::HttpRequestPtr& req,
                                              std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = static_cast<int>(numParam(req, "project_id"));
            if (projectId <= 0)
            {
                callback(errorResp("缺少 project_id"));
                return;
            }
            std::string analysis = strParam(req, "analysis", "");

            auto rows = terrainResultDao()->listByProject(projectId, analysis);
            Json::Value out(Json::arrayValue);
            for (const auto& r : rows)
            {
                // 对齐 Python: 过滤掉栅格文件已丢失的记录
                if (r.raster_path.empty() || !std::filesystem::exists(r.raster_path))
                    continue;
                Json::Value j;
                j["id"] = r.id;
                j["project_id"] = r.project_id;
                j["analysis"] = r.analysis;
                j["raster_path"] = r.raster_path;
                if (!r.stats_json.empty())
                {
                    Json::Value stats;
                    Json::Reader reader;
                    reader.parse(r.stats_json, stats, false);
                    j["stats"] = std::move(stats);
                }
                out.append(j);
            }
            callback(jsonResp(out));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

}  // namespace soilwatercm::controllers
