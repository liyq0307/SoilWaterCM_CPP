#define _USE_MATH_DEFINES
#include "acquisition_service.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <map>
#include <sstream>
#include <thread>

#include "../common/exceptions.h"
#include "../infrastructure/config_manager.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{

    namespace
    {
        constexpr double M_PER_DEG_LAT = 111320.0;

        double roundTo(double v, int digits)
        {
            double p = std::pow(10.0, digits);
            return std::round(v * p) / p;
        }

        std::string toLower(const std::string& s)
        {
            std::string r = s;
            std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return std::tolower(c); });
            return r;
        }

        std::string trimStr(const std::string& s)
        {
            size_t b = s.find_first_not_of(" \t\r\n");
            if (b == std::string::npos)
                return "";
            size_t e = s.find_last_not_of(" \t\r\n");
            return s.substr(b, e - b + 1);
        }

        std::string tpToString(const std::chrono::system_clock::time_point& tp)
        {
            auto t = std::chrono::system_clock::to_time_t(tp);
            std::tm tm = {};
            localtime_s(&tm, &t);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
            return buf;
        }

        // 从 body 提取 bbox: 支持 "bbox":[min_lon,min_lat,max_lon,max_lat] 或平铺 min_lon/... 字段
        std::array<double, 4> extractBbox(const nlohmann::json& body)
        {
            std::array<double, 4> bbox{0.0, 0.0, 0.0, 0.0};
            if (body.contains("bbox") && body["bbox"].is_array() && body["bbox"].size() == 4)
            {
                for (int i = 0; i < 4; ++i)
                    bbox[i] = body["bbox"][i].get<double>();
            }
            else
            {
                bbox = {body.value("min_lon", 0.0), body.value("min_lat", 0.0), body.value("max_lon", 0.0),
                        body.value("max_lat", 0.0)};
            }
            if (bbox[2] <= bbox[0] || bbox[3] <= bbox[1])
                throw common::ValidationException("bbox 非法 (max 须大于 min)");
            return bbox;
        }

        std::string bboxToString(const std::array<double, 4>& bbox)
        {
            std::ostringstream oss;
            oss << bbox[0] << "," << bbox[1] << "," << bbox[2] << "," << bbox[3];
            return oss.str();
        }

        // data_type -> 数据源 (sentinel / landsat / china), 对齐 Python collect_for_project 映射
        std::string sourceForDataType(const std::string& dt)
        {
            std::string low = toLower(dt);
            if (low.find("sentinel") != std::string::npos)
                return "sentinel";
            if (low.find("landsat") != std::string::npos)
                return "landsat";
            return "china";
        }

        // ---- 无人机相机预设 (对齐 flight_planning.py CAMERA_PRESETS) ----
        struct CameraPreset
        {
            std::string name;
            double focal_mm;
            double pixel_pitch_um;
            int img_w_px;
            int img_h_px;
        };

        const std::map<std::string, CameraPreset>& cameraPresets()
        {
            static const std::map<std::string, CameraPreset> presets = {
                {"phantom4_rtk", {"DJI Phantom 4 RTK", 8.8, 2.41, 5472, 3648}},
                {"mavic3e", {"DJI Mavic 3E", 12.29, 3.36, 5280, 3956}},
                {"p1_35mm", {"DJI Zenmuse P1 (35mm 全画幅)", 35.0, 4.4, 8192, 5460}},
                {"m3m", {"DJI Mavic 3M (多光谱)", 12.29, 3.36, 5280, 3956}},
            };
            return presets;
        }
    }  // namespace

    AcquisitionService::AcquisitionService(std::shared_ptr<infrastructure::external::SatelliteClient> satelliteClient,
                                           std::shared_ptr<dao::TaskDao> taskDao)
        : satelliteClient_(std::move(satelliteClient)), taskDao_(std::move(taskDao))
    {
    }

    nlohmann::json AcquisitionService::taskToJson(const models::Task& task)
    {
        nlohmann::json j;
        j["id"] = task.id;
        j["type"] = static_cast<int>(task.type);
        j["status"] = static_cast<int>(task.status);
        j["progress"] = task.progress;
        if (!task.params.empty())
        {
            try
            {
                j["params"] = nlohmann::json::parse(task.params);
            }
            catch (const std::exception&)
            {
                j["params"] = task.params;
            }
        }
        else
        {
            j["params"] = nullptr;
        }
        j["result_path"] = task.result_path.value_or(std::string());
        j["error_message"] = task.error_message.value_or(std::string());
        j["submitted_at"] = tpToString(task.submitted_at);
        j["started_at"] = task.started_at ? nlohmann::json(tpToString(*task.started_at)) : nlohmann::json(nullptr);
        j["completed_at"] =
            task.completed_at ? nlohmann::json(tpToString(*task.completed_at)) : nlohmann::json(nullptr);
        j["retry_count"] = task.retry_count;
        j["project_id"] = task.project_id;
        j["period_id"] = task.period_id;
        return j;
    }

    nlohmann::json AcquisitionService::searchSatellite(const nlohmann::json& params)
    {
        std::string source = toLower(trimStr(params.value("source", "")));
        if (source != "sentinel" && source != "landsat" && source != "china")
            throw common::ValidationException("未知数据源: " + source + " (可选 sentinel / landsat / china)");

        auto bbox = extractBbox(params);
        std::string startDate = params.value("start_date", "");
        std::string endDate = params.value("end_date", "");
        if (startDate.empty() || endDate.empty())
            throw common::ValidationException("缺少 start_date / end_date (格式 YYYY-MM-DD)");
        // 入参为百分比 (默认 20), SatelliteClient 用比例 (0~1)
        double cloudCover = params.value("cloud_cover", 20.0);
        double cloudFrac = cloudCover > 1.0 ? cloudCover / 100.0 : cloudCover;

        std::vector<std::string> imageIds;
        std::string bboxStr = bboxToString(bbox);
        try
        {
            if (source == "sentinel")
                imageIds = satelliteClient_->searchSentinel(bboxStr, startDate, endDate, cloudFrac);
            else if (source == "landsat")
                imageIds = satelliteClient_->searchLandsat(bboxStr, startDate, endDate, cloudFrac);
            else
                imageIds = satelliteClient_->searchCresda(bboxStr, startDate, endDate);
        }
        catch (const std::exception& e)
        {
            throw common::ExternalToolException(std::string("卫星检索失败 (检查数据源凭据): ") + e.what());
        }

        nlohmann::json scenes = nlohmann::json::array();
        for (const auto& id : imageIds)
        {
            nlohmann::json scene;
            scene["scene_id"] = id;
            try
            {
                auto meta = satelliteClient_->getMetadata(id);
                scene["datetime"] = meta.acquisition_time;
                scene["cloud_cover"] = meta.cloud_cover;
                scene["platform"] = meta.satellite_name;
                scene["sensor"] = meta.sensor;
                scene["orbit"] = meta.orbit;
                scene["projection"] = meta.projection;
                scene["bbox"] = meta.bbox;
            }
            catch (const std::exception& e)
            {
                scene["metadata_error"] = e.what();
            }
            scenes.push_back(scene);
        }

        nlohmann::json result;
        result["source"] = source;
        result["count"] = static_cast<int>(scenes.size());
        result["scenes"] = std::move(scenes);
        // 卫星数据源客户端当前为桩实现: 显式标注, 前端可提示"检索/采集未接入真实数据源"
        result["stub"] = true;
        result["note"] = "卫星检索客户端为桩实现 (C++ 后端未接入真实数据源), 结果为空";
        return result;
    }

    nlohmann::json AcquisitionService::collectSatellite(const nlohmann::json& params)
    {
        int projectId = params.value("project_id", 0);
        if (projectId <= 0)
            throw common::ValidationException("缺少 project_id");

        auto bbox = extractBbox(params);
        std::string startDate = params.value("start_date", "");
        std::string endDate = params.value("end_date", "");
        if (startDate.empty() || endDate.empty())
            throw common::ValidationException("缺少 start_date / end_date (格式 YYYY-MM-DD)");
        double cloudCover = params.value("cloud_cover", 20.0);

        // data_types: 逗号分隔字符串或数组; 留空默认 sentinel-2 + landsat
        std::vector<std::string> dataTypes;
        if (params.contains("data_types"))
        {
            if (params["data_types"].is_string())
            {
                std::stringstream ss(params["data_types"].get<std::string>());
                std::string item;
                while (std::getline(ss, item, ','))
                {
                    std::string t = trimStr(item);
                    if (!t.empty())
                        dataTypes.push_back(t);
                }
            }
            else if (params["data_types"].is_array())
            {
                for (const auto& d : params["data_types"])
                    if (d.is_string())
                        dataTypes.push_back(trimStr(d.get<std::string>()));
            }
        }
        if (dataTypes.empty())
            dataTypes = {"sentinel-2", "landsat"};

        nlohmann::json taskParams;
        taskParams["task_type"] = "collect_satellite";
        taskParams["bbox"] = bbox;
        taskParams["start_date"] = startDate;
        taskParams["end_date"] = endDate;
        taskParams["data_types"] = dataTypes;
        taskParams["cloud_cover"] = cloudCover;

        models::Task task;
        task.type = models::TaskType::Preprocess;  // 本项目 TaskType 无采集枚举, 归入预处理并用 params.task_type 区分
        task.status = models::TaskStatus::Pending;
        task.progress = 0.0;
        task.params = taskParams.dump();
        task.project_id = projectId;
        task.period_id = params.value("period_id", 0);
        task.retry_count = 0;
        task.submitted_at = std::chrono::system_clock::now();

        int taskId = taskDao_->insert(task);
        if (taskId < 0)
            throw common::DatabaseException("创建采集任务失败");
        task.id = taskId;

        // 后台线程异步执行检索 + 下载 (对齐 Celery task_collect_satellite)
        std::string bboxStr = bboxToString(bbox);
        double cloudFrac = cloudCover > 1.0 ? cloudCover / 100.0 : cloudCover;
        std::string outputDir = (std::filesystem::path(infrastructure::ConfigManager::instance().getStorageRawDir()) /
                                 ("project_" + std::to_string(projectId)) / "satellite")
                                    .string();

        std::thread([this, bboxStr, startDate, endDate, dataTypes, cloudFrac, outputDir, taskId]()
                    { runCollectTask(bboxStr, startDate, endDate, dataTypes, cloudFrac, outputDir, taskId); })
            .detach();

        return taskToJson(task);
    }

    void AcquisitionService::runCollectTask(std::string bboxStr, std::string startDate, std::string endDate,
                                            std::vector<std::string> dataTypes, double cloudCoverFrac,
                                            std::string outputDir, int taskId)
    {
        try
        {
            taskDao_->updateStatus(taskId, models::TaskStatus::Running, 10.0);
            std::filesystem::create_directories(outputDir);

            nlohmann::json collected = nlohmann::json::array();
            bool collectedAny = false;
            for (const auto& dt : dataTypes)
            {
                std::string source = sourceForDataType(dt);
                nlohmann::json item;
                item["type"] = dt;
                try
                {
                    std::vector<std::string> ids;
                    if (source == "sentinel")
                        ids = satelliteClient_->searchSentinel(bboxStr, startDate, endDate, cloudCoverFrac);
                    else if (source == "landsat")
                        ids = satelliteClient_->searchLandsat(bboxStr, startDate, endDate, cloudCoverFrac);
                    else
                        ids = satelliteClient_->searchCresda(bboxStr, startDate, endDate);

                    if (ids.empty())
                    {
                        item["status"] = "no_scenes";
                    }
                    else
                    {
                        const std::string& latest = ids.front();
                        item["scene_id"] = latest;
                        std::string sceneDir = (std::filesystem::path(outputDir) / latest).string();
                        std::string path = satelliteClient_->downloadImage(latest, sceneDir);
                        item["download_status"] = path.empty() ? "pending" : "downloaded";
                        item["output_path"] = sceneDir;
                        if (!path.empty())
                            collectedAny = true;
                    }
                }
                catch (const std::exception& e)
                {
                    utils::Logger::error("Acquisition: collect {} failed (task={}): {}", dt, taskId, e.what());
                    item["error"] = e.what();
                }
                collected.push_back(item);
            }

            taskDao_->updateResult(taskId, outputDir);
            if (collectedAny)
            {
                taskDao_->updateStatus(taskId, models::TaskStatus::Completed, 100.0);
                utils::Logger::info("Acquisition: collect task {} done, {} data types, out={}, detail={}", taskId,
                                    dataTypes.size(), outputDir, collected.dump());
            }
            else
            {
                // 桩客户端未采集到影像: 显式 Failed, 避免前端误判成功
                taskDao_->updateStatusAndMessage(
                    taskId, models::TaskStatus::Failed, 50.0,
                    "卫星采集客户端为桩实现 (C++ 后端未接入真实数据源), 未采集到影像");
                utils::Logger::warn("Acquisition: collect task {} marked failed (stub client, no scenes), out={}",
                                    taskId, outputDir);
            }
        }
        catch (const std::exception& e)
        {
            utils::Logger::error("Acquisition: collect task {} failed: {}", taskId, e.what());
            try
            {
                taskDao_->updateStatus(taskId, models::TaskStatus::Failed, 0.0);
            }
            catch (...)
            {
            }
        }
    }

    nlohmann::json AcquisitionService::generateFlightPlan(const nlohmann::json& params)
    {
        auto bbox = extractBbox(params);
        double gsdCm = params.value("gsd_cm", 3.0);
        std::string cameraKey = params.value("camera", "phantom4_rtk");

        auto it = cameraPresets().find(cameraKey);
        if (it == cameraPresets().end())
        {
            std::string names;
            for (const auto& [k, _] : cameraPresets())
                names += (names.empty() ? "" : ", ") + k;
            throw common::ValidationException("未知相机预设: " + cameraKey + " (可选 " + names + ")");
        }
        const CameraPreset& cam = it->second;

        // 重叠率缺省取 SL/T277 阈值 (航向 >=60%, 旁向 >=30%)
        double fwd = params.contains("forward_overlap") && params["forward_overlap"].is_number()
                         ? params["forward_overlap"].get<double>()
                         : 0.60;
        double side = params.contains("side_overlap") && params["side_overlap"].is_number()
                          ? params["side_overlap"].get<double>()
                          : 0.30;
        if (!(fwd >= 0.0 && fwd < 1.0 && side >= 0.0 && side < 1.0))
            throw common::ValidationException("重叠率须在 [0,1)");

        double minLon = bbox[0], minLat = bbox[1], maxLon = bbox[2], maxLat = bbox[3];

        // 目标 GSD -> 航高; 航高 -> 地面像幅
        double gsdM = gsdCm / 100.0;
        double heightM = gsdM * cam.focal_mm * 1000.0 / cam.pixel_pitch_um;
        double gsdBack = cam.pixel_pitch_um * heightM / (cam.focal_mm * 1000.0);
        double fwM = gsdBack * cam.img_w_px;
        double fhM = gsdBack * cam.img_h_px;

        // 航线间距 (旁向约束) / 拍照间隔 (航向约束)
        double lineSpacingM = fwM * (1.0 - side);
        double photoIntervalM = fhM * (1.0 - fwd);

        double centerLat = (minLat + maxLat) / 2.0;
        double mPerDegLon = M_PER_DEG_LAT * std::cos(centerLat * M_PI / 180.0);
        double widthM = (maxLon - minLon) * mPerDegLon;
        double heightSpanM = (maxLat - minLat) * M_PER_DEG_LAT;

        // 航带平行于长边 (减少转弯)
        bool linesAlongEw = widthM >= heightSpanM;
        double longM = std::max(widthM, heightSpanM);
        double shortM = std::min(widthM, heightSpanM);

        int nLines = std::max(static_cast<int>(std::ceil(shortM / lineSpacingM)) + 1, 1);
        int photosPerLine = std::max(static_cast<int>(std::ceil(longM / photoIntervalM)) + 1, 1);
        long long totalPhotos = static_cast<long long>(nLines) * photosPerLine;

        // 蛇形航带: 沿短边均布 nLines 条, 每条横跨长边, 奇数条反向
        nlohmann::json features = nlohmann::json::array();
        nlohmann::json waypoints = nlohmann::json::array();
        for (int i = 0; i < nLines; ++i)
        {
            double frac = nLines > 1 ? static_cast<double>(i) / (nLines - 1) : 0.0;
            std::array<double, 2> a, b;
            if (linesAlongEw)
            {
                double lat = minLat + frac * (maxLat - minLat);
                a = {minLon, lat};
                b = {maxLon, lat};
            }
            else
            {
                double lon = minLon + frac * (maxLon - minLon);
                a = {lon, minLat};
                b = {lon, maxLat};
            }
            if (i % 2 == 1)
                std::swap(a, b);

            nlohmann::json line;
            line["type"] = "Feature";
            line["properties"] = {{"line", i + 1}};
            line["geometry"] = {{"type", "LineString"},
                                {
                                    "coordinates",
                                    {{a[0], a[1]}, {b[0], b[1]}},
                                }};
            features.push_back(line);

            int seq = static_cast<int>(waypoints.size());
            waypoints.push_back({{"seq", seq + 1}, {"lon", a[0]}, {"lat", a[1]}});
            waypoints.push_back({{"seq", seq + 2}, {"lon", b[0]}, {"lat", b[1]}});
        }

        nlohmann::json result;
        result["camera"] = cam.name;
        result["gsd_cm"] = gsdCm;
        result["flight_height_m"] = roundTo(heightM, 1);
        result["footprint_m"] = {roundTo(fwM, 1), roundTo(fhM, 1)};
        result["forward_overlap"] = fwd;
        result["side_overlap"] = side;
        result["line_spacing_m"] = roundTo(lineSpacingM, 1);
        result["photo_interval_m"] = roundTo(photoIntervalM, 1);
        result["lines_direction"] = linesAlongEw ? "E-W" : "N-S";
        result["n_lines"] = nLines;
        result["photos_per_line"] = photosPerLine;
        result["total_photos"] = totalPhotos;
        result["area_ha"] = roundTo(widthM * heightSpanM / 10000.0, 2);
        result["flight_lines"] = {{"type", "FeatureCollection"}, {"features", features}};
        result["waypoints"] = waypoints;
        return result;
    }

    nlohmann::json AcquisitionService::backendStatus()
    {
        auto& cfg = infrastructure::ConfigManager::instance();
        std::string copId = cfg.getEnvVar("COPERNICUS_CLIENT_ID");
        std::string usgsUser = cfg.getEnvVar("USGS_USERNAME");
        std::string cresdaUser = cfg.getEnvVar("CRESDA_USERNAME");
        std::string cresdaPass = cfg.getEnvVar("CRESDA_PASSWORD");

        nlohmann::json result;
        result["sentinel"] = copId.empty() ? "未配置" : "Copernicus EODS";
        result["landsat"] = copId.empty()
                                ? (usgsUser.empty() ? "未配置 (需 USGS Earthdata 账号)" : "USGS EarthExplorer")
                                : "Copernicus EODS (STAC)";
        result["china"] = !cresdaUser.empty() && !cresdaPass.empty();
        result["client_healthy"] = satelliteClient_->isHealthy();
        result["note"] =
            "Copernicus EODS 免费注册: https://dataspace.copernicus.eu/ — 配置 COPERNICUS_CLIENT_ID/"
            "COPERNICUS_CLIENT_SECRET";
        return result;
    }

}  // namespace soilwatercm::services
