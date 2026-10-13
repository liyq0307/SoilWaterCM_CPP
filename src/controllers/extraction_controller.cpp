#include "extraction_controller.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <thread>

#include <drogon/HttpTypes.h>

#include "../common/exceptions.h"
#include "../infrastructure/config_manager.h"
#include "../models/common/enums.h"
#include "../utils/geojson_utils.h"
#include "../utils/logger.h"
#include <nlohmann/json.hpp>

namespace soilwatercm::controllers
{

    namespace
    {
        // 下方匿名命名空间实现的前置声明 (供既有 handler 分发使用)
        drogon::HttpResponsePtr jsonResp(const Json::Value& body,
                                         drogon::HttpStatusCode code = drogon::k200OK);
        drogon::HttpResponsePtr errorResp(const std::string& msg,
                                          drogon::HttpStatusCode code = drogon::k400BadRequest);
        Json::Value parseParams(const drogon::HttpRequestPtr& req);
        double getDbl(const Json::Value& p, const std::string& key, double fallback = 0.0);
        int getInt(const Json::Value& p, const std::string& key, int fallback = 0);
        std::string getStr(const Json::Value& p, const std::string& key, const std::string& fallback = "");
        models::MeasureType mapMeasureType(const std::string& category, const std::string& type);
        std::string geoJsonToWktChecked(const Json::Value& geom);
    }  // namespace

    ExtractionController::ExtractionController(std::shared_ptr<services::FvcService> fvcService,
                                               std::shared_ptr<services::ErosionService> erosionService,
                                               std::shared_ptr<services::DisturbanceService> disturbanceService,
                                               std::shared_ptr<services::MeasureService> measureService)
        : fvcService_(std::move(fvcService)),
          erosionService_(std::move(erosionService)),
          disturbanceService_(std::move(disturbanceService)),
          measureService_(std::move(measureService))
    {
    }

    void ExtractionController::extractFvc(const drogon::HttpRequestPtr& req,
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

        services::FvcParams params;
        params.period_id = (*json)["period_id"].asInt();

        if ((*json).isMember("image_ids"))
        {
            for (const auto& id : (*json)["image_ids"])
            {
                params.image_ids.push_back(id.asInt());
            }
        }

        auto future = fvcService_->submitFvcTask(params);

        Json::Value result;
        result["message"] = "FVC extraction task submitted";
        result["status"] = "pending";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k202Accepted);
        callback(resp);
    }

    void ExtractionController::extractErosion(const drogon::HttpRequestPtr& req,
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

        services::ErosionParams params;
        params.period_id = (*json)["period_id"].asInt();

        if ((*json).isMember("dem_path"))
        {
            params.dem_path = (*json)["dem_path"].asString();
        }

        auto future = erosionService_->submitErosionTask(params);

        Json::Value result;
        result["message"] = "Erosion calculation task submitted";
        result["status"] = "pending";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k202Accepted);
        callback(resp);
    }

    void ExtractionController::extractDisturbance(const drogon::HttpRequestPtr& req,
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

        services::DisturbanceParams params;
        params.period_id = (*json)["period_id"].asInt();

        if ((*json).isMember("before_image_path"))
        {
            params.before_image_path = (*json)["before_image_path"].asString();
        }

        if ((*json).isMember("after_image_path"))
        {
            params.after_image_path = (*json)["after_image_path"].asString();
        }

        auto future = disturbanceService_->submitDisturbanceTask(params);

        Json::Value result;
        result["message"] = "Disturbance extraction task submitted";
        result["status"] = "pending";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k202Accepted);
        callback(resp);
    }

    void ExtractionController::extractMeasures(const drogon::HttpRequestPtr& req,
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

        // 对齐 Python POST /extraction/measures (手动创建措施记录, 201):
        // 带 geom_geojson 视为创建请求; 否则保持原有任务提交语义 (路由同路径同方法, 无法双注册)
        if ((*json).isMember("geom_geojson"))
        {
            try
            {
                auto params = parseParams(req);
                int projectId = getInt(params, "project_id");
                int periodId = getInt(params, "period_id");
                if (projectId <= 0)
                {
                    callback(errorResp("缺少 project_id"));
                    return;
                }

                models::Measure m;
                m.type = mapMeasureType(getStr(params, "category"), getStr(params, "measure_type"));
                m.subtype = getStr(params, "measure_type");
                m.length = getDbl(params, "length_m", 0.0);
                m.area = getDbl(params, "area_m2", 0.0);
                m.status = getStr(params, "status", "active");
                m.condition = getStr(params, "condition", "good");
                m.period_id = periodId;
                m.geometry.wkt = geoJsonToWktChecked((*json)["geom_geojson"]);
                m.created_at = std::chrono::system_clock::now();

                int id = measureService_->createMeasure(m);
                if (id < 0)
                {
                    callback(errorResp("创建措施记录失败", drogon::k500InternalServerError));
                    return;
                }
                m.id = id;
                callback(jsonResp(measureToJson(m), drogon::k201Created));
            }
            catch (const common::ValidationException& e)
            {
                callback(errorResp(e.what(), drogon::k422UnprocessableEntity));
            }
            catch (const std::exception& e)
            {
                callback(errorResp(e.what(), drogon::k500InternalServerError));
            }
            return;
        }

        services::MeasureParams params;
        params.period_id = (*json)["period_id"].asInt();

        auto future = measureService_->submitMeasureTask(params);

        Json::Value result;
        result["message"] = "Measure interpretation task submitted";
        result["status"] = "pending";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k202Accepted);
        callback(resp);
    }

    // ==================== 以下为补齐端点 (对齐 Python endpoints/extraction.py) ====================

    namespace
    {
        drogon::HttpResponsePtr jsonResp(const Json::Value& body, drogon::HttpStatusCode code)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(code);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            Json::StreamWriterBuilder wb;
            wb["indentation"] = "";
            resp->setBody(Json::writeString(wb, body));
            return resp;
        }

        drogon::HttpResponsePtr errorResp(const std::string& msg, drogon::HttpStatusCode code)
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

        // 合并 JSON body 与 query 参数 (body 优先), 对齐 Python 端点 query 参数习惯
        Json::Value parseParams(const drogon::HttpRequestPtr& req)
        {
            Json::Value body = Json::objectValue;
            if (req->getJsonObject())
                body = *req->getJsonObject();

            for (const auto& key : {std::string("project_id"), std::string("period_id"), std::string("image_id"),
                                    std::string("before_image_id"), std::string("after_image_id"),
                                    std::string("baseline_period_id"), std::string("min_area_m2"),
                                    std::string("max_confidence"), std::string("top_n"), std::string("n_classes")})
            {
                if (body.isMember(key))
                    continue;
                std::string v = std::string(req->getParameter(key));
                if (v.empty())
                    continue;
                try
                {
                    size_t pos = 0;
                    double d = std::stod(v, &pos);
                    if (pos == v.size())
                        body[key] = d;
                }
                catch (const std::exception&)
                {
                }
            }
            for (const auto& key : {std::string("disturbance_type"), std::string("category"),
                                    std::string("measure_type"), std::string("method"), std::string("mode"),
                                    std::string("zone"), std::string("compliance"), std::string("stability_level"),
                                    std::string("model_path"), std::string("weights"), std::string("roi_path"),
                                    std::string("label_path"), std::string("dsm_image_id"), std::string("dem_image_id"),
                                    std::string("out_path"), std::string("source_methods"), std::string("prompt")})
            {
                if (body.isMember(key))
                    continue;
                std::string v = std::string(req->getParameter(key));
                if (!v.empty())
                    body[key] = v;
            }
            return body;
        }

        models::DisturbanceType mapDisturbanceType(const std::string& s)
        {
            if (s == "excavation" || s == "construction")
                return models::DisturbanceType::Excavation;
            if (s == "dump" || s == "dump_yard")
                return models::DisturbanceType::Dump;
            if (s == "spoil" || s == "borrow_pit")
                return models::DisturbanceType::Spoil;
            if (s == "temporary" || s == "temp_building")
                return models::DisturbanceType::Temporary;
            return models::DisturbanceType::Other;
        }

        std::string disturbanceTypeName(models::DisturbanceType t)
        {
            switch (t)
            {
            case models::DisturbanceType::Excavation:
                return "excavation";
            case models::DisturbanceType::Dump:
                return "dump";
            case models::DisturbanceType::Spoil:
                return "spoil";
            case models::DisturbanceType::Temporary:
                return "temporary";
            default:
                return "other";
            }
        }

        models::MeasureType mapMeasureType(const std::string& category, const std::string& type)
        {
            if (category == "vegetation" || category == "plant" || type == "revegetation" || type == "grass_planting")
                return models::MeasureType::Plant;
            if (category == "temporary")
                return models::MeasureType::Temporary;
            return models::MeasureType::Engineering;
        }

        std::string measureCategoryName(models::MeasureType t)
        {
            switch (t)
            {
            case models::MeasureType::Plant:
                return "vegetation";
            case models::MeasureType::Temporary:
                return "temporary";
            default:
                return "engineering";
            }
        }

        // GeoJSON geometry (Json::Value) -> WKT; 失败抛异常
        std::string geoJsonToWktChecked(const Json::Value& geom)
        {
            Json::StreamWriterBuilder wb;
            wb["indentation"] = "";
            auto nj = nlohmann::json::parse(Json::writeString(wb, geom));
            std::string wkt = utils::GeoJsonUtils::geoJsonToWkt(nj);
            if (wkt.empty())
                throw common::ValidationException("geom_geojson 无法解析为有效几何");
            return wkt;
        }

        // jsoncpp 取值助手
        double getDbl(const Json::Value& p, const std::string& key, double fallback)
        {
            if (!p.isMember(key) || p[key].isNull())
                return fallback;
            if (p[key].isNumeric())
                return p[key].asDouble();
            if (p[key].isString())
            {
                try
                {
                    return std::stod(p[key].asString());
                }
                catch (const std::exception&)
                {
                    return fallback;
                }
            }
            return fallback;
        }

        int getInt(const Json::Value& p, const std::string& key, int fallback)
        {
            return static_cast<int>(getDbl(p, key, static_cast<double>(fallback)));
        }

        std::string getStr(const Json::Value& p, const std::string& key, const std::string& fallback)
        {
            if (!p.isMember(key) || p[key].isNull())
                return fallback;
            if (p[key].isString())
                return p[key].asString();
            return fallback;
        }

        Json::Value taskToJson(const models::Task& task)
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
    }  // namespace

    std::shared_ptr<repository::PostgresRepository> ExtractionController::sharedRepo()
    {
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

    std::shared_ptr<dao::TaskDao> ExtractionController::taskDao()
    {
        if (!taskDao_)
            taskDao_ = std::make_shared<dao::TaskDao>(sharedRepo());
        return taskDao_;
    }

    std::shared_ptr<dao::ImageDao> ExtractionController::imageDao()
    {
        if (!imageDao_)
            imageDao_ = std::make_shared<dao::ImageDao>(sharedRepo());
        return imageDao_;
    }

    std::shared_ptr<dao::AnomalyDao> ExtractionController::anomalyDao()
    {
        if (!anomalyDao_)
            anomalyDao_ = std::make_shared<dao::AnomalyDao>(sharedRepo());
        return anomalyDao_;
    }

    std::shared_ptr<dao::LandUseResultDao> ExtractionController::landuseResultDao()
    {
        if (!landuseResultDao_)
            landuseResultDao_ = std::make_shared<dao::LandUseResultDao>(sharedRepo());
        return landuseResultDao_;
    }

    std::shared_ptr<dao::SegmentationResultDao> ExtractionController::segmentationResultDao()
    {
        if (!segmentationResultDao_)
            segmentationResultDao_ = std::make_shared<dao::SegmentationResultDao>(sharedRepo());
        return segmentationResultDao_;
    }

    std::shared_ptr<dao::SpoilDumpDao> ExtractionController::spoilDumpDao()
    {
        if (!spoilDumpDao_)
            spoilDumpDao_ = std::make_shared<dao::SpoilDumpDao>(sharedRepo());
        return spoilDumpDao_;
    }

    std::shared_ptr<dao::TrainedModelDao> ExtractionController::trainedModelDao()
    {
        if (!trainedModelDao_)
            trainedModelDao_ = std::make_shared<dao::TrainedModelDao>(sharedRepo());
        return trainedModelDao_;
    }

    std::shared_ptr<dao::MeasureDao> ExtractionController::measureDao()
    {
        if (!measureDao_)
            measureDao_ = std::make_shared<dao::MeasureDao>(sharedRepo());
        return measureDao_;
    }

    Json::Value ExtractionController::disturbanceToJson(const models::Disturbance& d)
    {
        Json::Value j;
        j["id"] = d.id;
        j["period_id"] = d.period_id;
        j["disturbance_type"] = disturbanceTypeName(d.type);
        j["disturbance_code"] = d.disturbance_code;
        j["area_m2"] = d.area;
        j["perimeter_m"] = d.perimeter;
        j["confidence"] = d.confidence;
        j["created_at"] = tpToStr(d.created_at);
        if (!d.geometry.wkt.empty())
        {
            auto geom = utils::GeoJsonUtils::wktToGeoJson(d.geometry.wkt);
            if (!geom.is_null())
            {
                std::string s = geom.dump();
                Json::CharReaderBuilder rb;
                Json::Value gv;
                std::string errs;
                std::istringstream iss(s);
                if (Json::parseFromStream(rb, iss, &gv, &errs))
                    j["geom"] = gv;
            }
        }
        // C++ 模型无 zone/compliance/extract_method/note 字段 (与 Python 偏差)
        return j;
    }

    Json::Value ExtractionController::measureToJson(const models::Measure& m)
    {
        Json::Value j;
        j["id"] = m.id;
        j["period_id"] = m.period_id;
        j["category"] = measureCategoryName(m.type);
        j["measure_type"] = m.subtype.empty() ? measureCategoryName(m.type) : m.subtype;
        j["length_m"] = m.length;
        j["area_m2"] = m.area;
        j["status"] = m.status;
        j["condition"] = m.condition;
        if (m.vegetation_coverage.has_value())
            j["vegetation_coverage"] = *m.vegetation_coverage;
        else
            j["vegetation_coverage"] = Json::nullValue;
        j["created_at"] = tpToStr(m.created_at);
        if (!m.geometry.wkt.empty())
        {
            auto geom = utils::GeoJsonUtils::wktToGeoJson(m.geometry.wkt);
            if (!geom.is_null())
            {
                std::string s = geom.dump();
                Json::CharReaderBuilder rb;
                Json::Value gv;
                std::string errs;
                std::istringstream iss(s);
                if (Json::parseFromStream(rb, iss, &gv, &errs))
                    j["geom"] = gv;
            }
        }
        return j;
    }

    models::Task ExtractionController::createAiTask(const std::string& taskType, int projectId, int periodId,
                                                    const Json::Value& taskParams)
    {
        Json::Value p = taskParams;
        p["task_type"] = taskType;

        models::Task task;
        task.type = models::TaskType::Preprocess;  // 本项目 TaskType 枚举不足, 以 params.task_type 区分
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

    void ExtractionController::listDisturbances(const drogon::HttpRequestPtr& req,
                                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            auto params = parseParams(req);
            int projectId = getInt(params, "project_id");
            if (projectId <= 0)
            {
                callback(errorResp("缺少 project_id"));
                return;
            }
            std::optional<int> periodId;
            if (getInt(params, "period_id") > 0)
                periodId = getInt(params, "period_id");

            Json::Value out(Json::arrayValue);
            for (const auto& d : disturbanceService_->getDisturbancesByProject(projectId, periodId))
                out.append(disturbanceToJson(d));
            callback(jsonResp(out));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ExtractionController::createDisturbance(const drogon::HttpRequestPtr& req,
                                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            auto params = parseParams(req);
            int projectId = getInt(params, "project_id");
            int periodId = getInt(params, "period_id");
            if (projectId <= 0 || periodId <= 0)
            {
                callback(errorResp("缺少 project_id / period_id"));
                return;
            }
            if (!params.isMember("geom_geojson"))
            {
                callback(errorResp("缺少 geom_geojson"));
                return;
            }

            models::Disturbance d;
            d.disturbance_code = "MANUAL_" + std::to_string(periodId) + "_" +
                                 std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                                                    std::chrono::system_clock::now().time_since_epoch())
                                                    .count());
            d.type = mapDisturbanceType(getStr(params, "disturbance_type", "other"));
            d.area = getDbl(params, "area_m2", 0.0);
            d.perimeter = getDbl(params, "perimeter_m", 0.0);
            d.confidence = getDbl(params, "confidence", 1.0);
            d.period_id = periodId;
            d.geometry.wkt = geoJsonToWktChecked(params["geom_geojson"]);
            d.created_at = std::chrono::system_clock::now();

            int id = disturbanceService_->createDisturbance(d);
            if (id < 0)
            {
                callback(errorResp("创建扰动记录失败", drogon::k500InternalServerError));
                return;
            }
            d.id = id;
            // 与 Python 偏差: C++ Disturbance 无 project_id 列 (经 period join 归属项目)
            callback(jsonResp(disturbanceToJson(d), drogon::k201Created));
        }
        catch (const common::ValidationException& e)
        {
            callback(errorResp(e.what(), drogon::k422UnprocessableEntity));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ExtractionController::autoExtractDisturbances(const drogon::HttpRequestPtr& req,
                                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            auto params = parseParams(req);
            int projectId = getInt(params, "project_id");
            int periodId = getInt(params, "period_id");
            int imageId = getInt(params, "image_id");
            if (projectId <= 0 || periodId <= 0 || imageId <= 0)
            {
                callback(errorResp("缺少 project_id / period_id / image_id"));
                return;
            }

            auto current = imageDao()->findById(imageId);
            if (!current)
            {
                callback(errorResp("影像不存在: " + std::to_string(imageId), drogon::k404NotFound));
                return;
            }

            // 对齐 Python _select_reference_image: 同项目 acquisition_time 早于当期的最近影像做双期对比,
            // 无则回退单期 (before==after, C++ 变化检测链路对同影像输出空集)
            std::string beforePath = current->file_path;
            std::string reference = "single_image_fallback";
            std::optional<std::chrono::system_clock::time_point> bestTs;
            for (const auto& img : imageDao()->findByProjectId(projectId))
            {
                if (img.id == imageId || img.file_path.empty())
                    continue;
                if (!(img.acquisition_time < current->acquisition_time))
                    continue;
                if (!bestTs || *bestTs < img.acquisition_time)
                {
                    bestTs = img.acquisition_time;
                    beforePath = img.file_path;
                    reference = "latest_previous";
                }
            }

            services::DisturbanceParams dParams;
            dParams.period_id = periodId;
            dParams.before_image_path = beforePath;
            dParams.after_image_path = current->file_path;
            dParams.output_dir = (std::filesystem::path(
                                      infrastructure::ConfigManager::instance().getStorageResultsDir()) /
                                  "disturbance" / ("p" + std::to_string(projectId)))
                                     .string();
            dParams.min_area = getDbl(params, "min_area_m2", 100.0) / 10000.0;  // m2 -> ha (C++ 阈值语义)

            Json::Value taskParams;
            taskParams["image_id"] = imageId;
            taskParams["selection"] = reference;
            taskParams["min_area_m2"] = getDbl(params, "min_area_m2", 100.0);
            auto task = createAiTask("auto_extract_disturbance", projectId, periodId, taskParams);

            std::thread([this, dParams, taskId = task.id]()
                        { disturbanceService_->executeDisturbance(dParams, taskId); })
                .detach();

            callback(jsonResp(taskToJson(task)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ExtractionController::listMeasures(const drogon::HttpRequestPtr& req,
                                            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            auto params = parseParams(req);
            int projectId = getInt(params, "project_id");
            if (projectId <= 0)
            {
                callback(errorResp("缺少 project_id"));
                return;
            }
            std::optional<int> periodId;
            if (params.isMember("period_id") && params["period_id"].isNumeric() && params["period_id"].asDouble() > 0)
                periodId = static_cast<int>(params["period_id"].asDouble());

            Json::Value out(Json::arrayValue);
            for (const auto& m : measureService_->getMeasuresByProject(projectId, periodId))
                out.append(measureToJson(m));
            callback(jsonResp(out));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ExtractionController::autoInterpretMeasures(const drogon::HttpRequestPtr& req,
                                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            auto params = parseParams(req);
            int projectId = getInt(params, "project_id");
            int periodId = getInt(params, "period_id");
            int imageId = getInt(params, "image_id");
            if (projectId <= 0 || periodId <= 0 || imageId <= 0)
            {
                callback(errorResp("缺少 project_id / period_id / image_id"));
                return;
            }

            std::string imagePath;
            auto img = imageDao()->findById(imageId);
            if (img)
                imagePath = img->file_path;

            services::MeasureParams mParams;
            mParams.period_id = periodId;
            mParams.image_path = imagePath;
            mParams.output_dir = (std::filesystem::path(
                                      infrastructure::ConfigManager::instance().getStorageResultsDir()) /
                                  "measures" / ("p" + std::to_string(projectId)))
                                     .string();

            Json::Value taskParams;
            taskParams["image_id"] = imageId;
            if (params.isMember("baseline_period_id"))
                taskParams["baseline_period_id"] = params["baseline_period_id"];
            auto task = createAiTask("interpret_measures", projectId, periodId, taskParams);

            // measureService 已有半自动解译链路 (detectMeasures + 落库)
            std::thread([this, mParams, taskId = task.id]() { measureService_->executeMeasure(mParams, taskId); })
                .detach();

            callback(jsonResp(taskToJson(task)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ExtractionController::getMeasure(const drogon::HttpRequestPtr& /*req*/,
                                          std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        try
        {
            auto mOpt = measureDao()->findById(id);
            if (!mOpt)
            {
                callback(errorResp("措施不存在", drogon::k404NotFound));
                return;
            }
            callback(jsonResp(measureToJson(*mOpt)));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ExtractionController::deleteMeasure(const drogon::HttpRequestPtr& /*req*/,
                                             std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        try
        {
            auto mOpt = measureDao()->findById(id);
            if (!mOpt)
            {
                callback(errorResp("措施不存在", drogon::k404NotFound));
                return;
            }
            if (!measureDao()->remove(id))
            {
                callback(errorResp("删除措施失败", drogon::k500InternalServerError));
                return;
            }
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k204NoContent);
            callback(resp);
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ExtractionController::listLanduse(const drogon::HttpRequestPtr& req,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            auto params = parseParams(req);
            int projectId = getInt(params, "project_id");
            if (projectId <= 0)
            {
                callback(errorResp("缺少 project_id"));
                return;
            }
            std::optional<int> periodId;
            if (getInt(params, "period_id") > 0)
                periodId = getInt(params, "period_id");

            auto rows = landuseResultDao()->findByProject(projectId, periodId);

            Json::Value out(Json::arrayValue);
            for (const auto& r : rows)
            {
                Json::Value j;
                j["id"] = r.id;
                j["project_id"] = r.project_id;
                j["period_id"] = r.period_id ? *r.period_id : Json::nullValue;
                j["image_id"] = r.image_id ? *r.image_id : Json::nullValue;
                j["method"] = r.method;
                j["n_classes"] = r.n_classes ? *r.n_classes : Json::nullValue;
                j["raster_path"] = r.raster_path;
                if (!r.class_areas_json.empty())
                {
                    Json::Value cv;
                    Json::Reader reader;
                    reader.parse(r.class_areas_json, cv, false);
                    j["class_areas"] = cv;
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

    void ExtractionController::listSegments(const drogon::HttpRequestPtr& req,
                                            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            auto params = parseParams(req);
            std::optional<int> imageId;
            if (getInt(params, "image_id") > 0)
                imageId = getInt(params, "image_id");
            std::optional<int> projectId;
            if (getInt(params, "project_id") > 0)
                projectId = getInt(params, "project_id");

            auto rows = segmentationResultDao()->findByFilter(imageId, projectId);

            Json::Value out(Json::arrayValue);
            for (const auto& r : rows)
            {
                Json::Value j;
                j["id"] = r.id;
                j["image_id"] = r.image_id;
                j["project_id"] = r.project_id ? *r.project_id : Json::nullValue;
                j["period_id"] = r.period_id ? *r.period_id : Json::nullValue;
                j["method"] = r.method;
                j["n_segments"] = r.n_segments ? *r.n_segments : Json::nullValue;
                j["raster_path"] = r.raster_path;
                out.append(j);
            }
            callback(jsonResp(out));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ExtractionController::listSpoilDumps(const drogon::HttpRequestPtr& req,
                                              std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            auto params = parseParams(req);
            int projectId = getInt(params, "project_id");
            if (projectId <= 0)
            {
                callback(errorResp("缺少 project_id"));
                return;
            }
            std::optional<int> periodId;
            if (getInt(params, "period_id") > 0)
                periodId = getInt(params, "period_id");
            std::string compliance = getStr(params, "compliance");
            std::string stability = getStr(params, "stability_level");

            auto rows = spoilDumpDao()->findByProject(projectId, periodId, compliance, stability);

            Json::Value out(Json::arrayValue);
            for (const auto& s : rows)
            {
                Json::Value j;
                j["id"] = s.id;
                j["project_id"] = s.project_id;
                j["period_id"] = s.period_id;
                j["image_id"] = s.image_id ? *s.image_id : Json::nullValue;
                j["area_m2"] = s.area_m2;
                j["volume_m3"] = s.volume_m3 ? *s.volume_m3 : Json::nullValue;
                j["max_height_m"] = s.max_height_m ? *s.max_height_m : Json::nullValue;
                j["mean_height_m"] = s.mean_height_m ? *s.mean_height_m : Json::nullValue;
                j["in_approved_area"] = s.in_approved_area ? *s.in_approved_area : Json::nullValue;
                j["has_protection"] = s.has_protection ? *s.has_protection : Json::nullValue;
                j["stability_level"] = s.stability_level;
                j["risk_score"] = s.risk_score ? *s.risk_score : Json::nullValue;
                j["compliance"] = s.compliance;
                j["detect_method"] = s.detect_method;
                j["confidence"] = s.confidence ? *s.confidence : Json::nullValue;
                out.append(j);
            }
            callback(jsonResp(out));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ExtractionController::autoDetectSpoilDumps(const drogon::HttpRequestPtr& req,
                                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            auto params = parseParams(req);
            int projectId = getInt(params, "project_id");
            int periodId = getInt(params, "period_id");
            if (projectId <= 0 || periodId <= 0)
            {
                callback(errorResp("缺少 project_id / period_id"));
                return;
            }

            // DSM/DEM 差值堆高算法未移植: 建任务占位, 后台置失败并说明
            auto task = createAiTask("detect_spoil_dump", projectId, periodId, params);
            int taskId = task.id;
            std::thread([this, taskId]()
                        {
                            try
                            {
                                taskDao()->updateStatusAndMessage(taskId, models::TaskStatus::Failed, 0.0,
                                                                  "弃渣场 DSM 差值识别算法未移植");
                            }
                            catch (...)
                            {
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

    void ExtractionController::submitAiPlaceholder(const drogon::HttpRequestPtr& req,
                                                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                                   const std::string& taskType)
    {
        // AI 训练/预测类端点统一入口: 本项目无 Python AI 运行时, 建 pending 任务后
        // 后台线程标记 failed 并写 "AI 后端未移植" 说明 (选择 failed 而非保持 pending,
        // 避免前端无限轮询等待)
        try
        {
            auto params = parseParams(req);
            int projectId = getInt(params, "project_id");
            int periodId = getInt(params, "period_id");
            if (projectId <= 0)
            {
                callback(errorResp("缺少 project_id"));
                return;
            }
            auto task = createAiTask(taskType, projectId, periodId, params);
            int taskId = task.id;
            std::thread([this, taskId, taskType]()
                        {
                            try
                            {
                                taskDao()->updateStatusAndMessage(taskId, models::TaskStatus::Failed, 0.0,
                                                                  taskType + ": AI 后端未移植 (无 Python 运行时)");
                            }
                            catch (...)
                            {
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

    void ExtractionController::submitClassify(const drogon::HttpRequestPtr& req,
                                              std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        // 土地利用分类 (kmeans/rf/cnn 等) 依赖 Python AI 运行时, 未移植: 任务占位
        submitAiPlaceholder(req, std::move(callback), "classify_landuse");
    }

    void ExtractionController::submitSegment(const drogon::HttpRequestPtr& req,
                                             std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        // 影像分割 (mean_shift/watershed/superpixel) 未移植: 任务占位
        submitAiPlaceholder(req, std::move(callback), "segment");
    }

    void ExtractionController::trainChange(const drogon::HttpRequestPtr& req,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        // 变化检测模型训练 (bit/changeformer) 未移植: 任务占位
        submitAiPlaceholder(req, std::move(callback), "train_change_model");
    }

    void ExtractionController::predictChange(const drogon::HttpRequestPtr& req,
                                             std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        // 变化检测模型推理未移植: 任务占位
        submitAiPlaceholder(req, std::move(callback), "predict_change");
    }

    void ExtractionController::latestTrainedModel(const drogon::HttpRequestPtr& req,
                                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            auto params = parseParams(req);
            int projectId = getInt(params, "project_id");
            if (projectId <= 0)
            {
                callback(errorResp("缺少 project_id"));
                return;
            }

            Json::Value out;
            out["task_id"] = Json::nullValue;
            out["model_exists"] = false;
            out["models"] = Json::Value(Json::arrayValue);

            auto models = trainedModelDao()->findByProject(projectId, false);
            Json::Value arr = out["models"];
            for (const auto& m : models)
            {
                Json::Value j;
                j["id"] = m.id;
                j["method"] = m.method;
                j["model_name"] = m.model_name;
                j["model_path"] = m.model_path;
                j["status"] = m.status;
                j["source_task_id"] = m.source_task_id ? (Json::Int64)*m.source_task_id : Json::nullValue;
                arr.append(j);
                // 最新 (列表按 created_at DESC) 作为兼容字段
                if (!out["model_exists"].asBool())
                {
                    out["model_exists"] = true;
                    out["model_name"] = m.model_name;
                    out["model_path"] = m.model_path;
                    out["method"] = m.method;
                    if (m.source_task_id)
                        out["task_id"] = (Json::Int64)*m.source_task_id;
                }
            }
            out["models"] = arr;
            callback(jsonResp(out));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ExtractionController::finetuneResources(const drogon::HttpRequestPtr& /*req*/,
                                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        // 微调 tile/权重盘点依赖 Python FLAIR 数据集目录, 未移植: 空盘点
        Json::Value out;
        out["tiles"] = Json::Value(Json::arrayValue);
        out["labels"] = Json::Value(Json::arrayValue);
        out["weights"] = Json::Value(Json::arrayValue);
        callback(jsonResp(out));
    }

    void ExtractionController::reviewCandidates(const drogon::HttpRequestPtr& req,
                                                std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            auto params = parseParams(req);
            int projectId = getInt(params, "project_id");
            if (projectId <= 0)
            {
                callback(errorResp("缺少 project_id"));
                return;
            }
            double maxConfidence = getDbl(params, "max_confidence", 0.4);
            int topN = getInt(params, "top_n", 200);
            std::optional<int> periodId;
            if (params.isMember("period_id") && params["period_id"].isNumeric() && params["period_id"].asDouble() > 0)
                periodId = static_cast<int>(params["period_id"].asDouble());

            // 低置信度自动图斑 -> 复核候选 (置信度升序); C++ 模型无 extract_method,
            // 以 confidence < 1.0 视为自动图斑候选 (与 Python 偏差)
            auto all = disturbanceService_->getDisturbancesByProject(projectId, periodId);
            std::vector<models::Disturbance> candidates;
            for (const auto& d : all)
            {
                if (d.confidence <= maxConfidence)
                    candidates.push_back(d);
            }
            std::sort(candidates.begin(), candidates.end(),
                      [](const models::Disturbance& a, const models::Disturbance& b)
                      { return a.confidence < b.confidence; });
            if (static_cast<int>(candidates.size()) > topN)
                candidates.resize(topN);

            Json::Value out(Json::arrayValue);
            for (const auto& d : candidates)
                out.append(disturbanceToJson(d));
            callback(jsonResp(out));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ExtractionController::learningStats(const drogon::HttpRequestPtr& req,
                                             std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            auto params = parseParams(req);
            int projectId = getInt(params, "project_id");
            if (projectId <= 0)
            {
                callback(errorResp("缺少 project_id"));
                return;
            }
            double maxConfidence = getDbl(params, "max_confidence", 0.4);
            std::optional<int> periodId;
            if (getInt(params, "period_id") > 0)
                periodId = getInt(params, "period_id");

            // C++ 模型无 extract_method / ground_truth 回流统计, 提供简化聚合 (与 Python 偏差)
            auto all = disturbanceService_->getDisturbancesByProject(projectId, periodId);
            int lowConf = 0;
            for (const auto& d : all)
                if (d.confidence <= maxConfidence)
                    lowConf++;

            Json::Value out;
            out["project_id"] = projectId;
            out["total_disturbances"] = static_cast<int>(all.size());
            out["pending_review"] = lowConf;
            out["by_method"] = Json::nullValue;
            out["note"] = "按方法统计/误报回流依赖 extract_method 与 ground_truth 表, C++ 模型未含该字段";
            callback(jsonResp(out));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

    void ExtractionController::reviewDisturbance(const drogon::HttpRequestPtr& req,
                                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                                 int id)
    {
        try
        {
            auto params = parseParams(req);
            std::string action = getStr(params, "action");
            if (action != "confirm" && action != "false_alarm")
            {
                callback(errorResp("action 必须是 confirm/false_alarm", drogon::k422UnprocessableEntity));
                return;
            }

            auto dOpt = disturbanceService_->getDisturbanceById(id);
            if (!dOpt)
            {
                callback(errorResp("图斑不存在: " + std::to_string(id), drogon::k404NotFound));
                return;
            }
            models::Disturbance d = *dOpt;

            if (action == "confirm")
            {
                // 确认: 置信度置满, 退出待复核清单
                d.confidence = 1.0;
                disturbanceService_->updateDisturbance(d);
                Json::Value out;
                out["id"] = d.id;
                out["action"] = "confirm";
                out["confidence"] = 1.0;
                callback(jsonResp(out));
                return;
            }

            // 误报: 图斑移出扰动成果, 几何留档为异常记录 (Python 写 Anomaly.status=false_alarm,
            // 本项目 DisposalStatus 无 false_alarm 枚举, 以 Pending 记录并在 description 标注)
            models::Anomaly a;
            a.type = models::AnomalyType::DisturbanceViolation;
            a.severity = models::Severity::Low;
            a.description = "扰动复核误报 (" + disturbanceTypeName(d.type) +
                            "): 低置信度自动图斑人工复核判定误报 (原置信度 " + std::to_string(d.confidence) + ")";
            a.status = models::DisposalStatus::Pending;
            a.geometry = d.geometry;
            a.detected_at = std::chrono::system_clock::now();
            a.project_id = 0;  // 与 Python 偏差: C++ Disturbance 无 project_id 列, 无法直接继承
            a.period_id = d.period_id;
            anomalyDao()->insert(a);
            disturbanceService_->deleteDisturbance(id);

            Json::Value out;
            out["id"] = id;
            out["action"] = "false_alarm";
            callback(jsonResp(out));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what(), drogon::k500InternalServerError));
        }
    }

}  // namespace soilwatercm::controllers
