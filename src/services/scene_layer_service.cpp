#include "scene_layer_service.h"

#include <algorithm>
#include <filesystem>
#include <ctime>

#include "../utils/geojson_utils.h"

namespace soilwatercm::services
{

    namespace
    {
        // 本项目枚举列用 int 存储; properties 输出 snake_case 字符串对齐 Python 字段
        std::string disturbanceTypeToString(models::DisturbanceType t)
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
                case models::DisturbanceType::Other:
                default:
                    return "other";
            }
        }

        std::string measureCategoryToString(models::MeasureType t)
        {
            switch (t)
            {
                case models::MeasureType::Engineering:
                    return "engineering";
                case models::MeasureType::Plant:
                    return "vegetation";
                case models::MeasureType::Temporary:
                default:
                    return "temporary";
            }
        }

        std::string anomalyTypeToString(models::AnomalyType t)
        {
            switch (t)
            {
                case models::AnomalyType::DisturbanceViolation:
                    return "illegal_disturbance";
                case models::AnomalyType::SlopeCollapse:
                    return "slope_collapse";
                case models::AnomalyType::SpoilLoss:
                    return "dump_loss";
                case models::AnomalyType::InsarDeformation:
                    return "insar_deformation";
                case models::AnomalyType::ErosionGully:
                default:
                    return "erosion_gully";
            }
        }

        std::string severityToString(models::Severity s)
        {
            switch (s)
            {
                case models::Severity::Low:
                    return "low";
                case models::Severity::Medium:
                    return "medium";
                case models::Severity::High:
                    return "high";
                case models::Severity::Critical:
                default:
                    return "critical";
            }
        }

        std::string disposalStatusToString(models::DisposalStatus s)
        {
            switch (s)
            {
                case models::DisposalStatus::Pending:
                    return "pending";
                case models::DisposalStatus::Assigned:
                    return "assigned";
                case models::DisposalStatus::Processing:
                    return "processing";
                case models::DisposalStatus::Completed:
                default:
                    return "completed";
            }
        }

        nlohmann::json emptyFeatureCollection()
        {
            return utils::GeoJsonUtils::makeFeatureCollection(nlohmann::json::array());
        }

        // WKT -> GeoJSON geometry; 失败返回 nullptr
        nlohmann::json geometryOf(const models::Geometry& geom)
        {
            return utils::GeoJsonUtils::wktToGeoJson(geom.wkt);
        }

        // 时间戳 -> ISO8601 (UTC), 对齐 created_at 输出
        std::string isoTime(std::chrono::system_clock::time_point tp)
        {
            auto time = std::chrono::system_clock::to_time_t(tp);
            std::tm tm = {};
#if defined(_WIN32)
            gmtime_s(&tm, &time);
#else
            gmtime_r(&time, &tm);
#endif
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
            return buf;
        }
    }  // namespace

    SceneLayerService::SceneLayerService(std::shared_ptr<dao::DisturbanceDao> disturbanceDao,
                                         std::shared_ptr<dao::MeasureDao> measureDao,
                                         std::shared_ptr<dao::AnomalyDao> anomalyDao,
                                         std::shared_ptr<dao::MonitoringPeriodDao> periodDao,
                                         std::shared_ptr<dao::VegetationCoverageDao> fvcDao,
                                         std::shared_ptr<dao::ErosionResultDao> erosionDao,
                                         std::shared_ptr<dao::TerrainResultDao> terrainDao,
                                         std::shared_ptr<dao::SpoilDumpDao> spoilDao,
                                         std::shared_ptr<infrastructure::gdal::VectorProcessor> vectorProcessor)
        : disturbanceDao_(std::move(disturbanceDao)), measureDao_(std::move(measureDao)),
          anomalyDao_(std::move(anomalyDao)), periodDao_(std::move(periodDao)), fvcDao_(std::move(fvcDao)),
          erosionDao_(std::move(erosionDao)), terrainDao_(std::move(terrainDao)), spoilDao_(std::move(spoilDao)),
          vectorProcessor_(std::move(vectorProcessor))
    {
    }

    std::vector<int> SceneLayerService::periodIdsForProject(int projectId)
    {
        std::vector<int> ids;
        for (const auto& p : periodDao_->findByProjectId(projectId))
            ids.push_back(p.id);
        return ids;
    }

    bool SceneLayerService::periodBelongsToProject(int projectId, std::optional<int> periodId)
    {
        if (!periodId.has_value())
            return true;
        auto period = periodDao_->findById(*periodId);
        return period.has_value() && period->project_id == projectId;
    }

    nlohmann::json SceneLayerService::disturbances(int projectId, std::optional<int> periodId)
    {
        std::vector<models::Disturbance> rows;
        if (periodId.has_value())
        {
            if (!periodBelongsToProject(projectId, periodId))
                return emptyFeatureCollection();
            rows = disturbanceDao_->findByPeriodId(*periodId);
        }
        else
        {
            for (int pid : periodIdsForProject(projectId))
            {
                auto part = disturbanceDao_->findByPeriodId(pid);
                rows.insert(rows.end(), part.begin(), part.end());
            }
        }

        // 对齐 Python: ORDER BY area_m2 DESC LIMIT 2000
        std::sort(rows.begin(), rows.end(),
                  [](const models::Disturbance& a, const models::Disturbance& b) { return a.area > b.area; });
        if (rows.size() > 2000)
            rows.resize(2000);

        nlohmann::json features = nlohmann::json::array();
        for (const auto& d : rows)
        {
            nlohmann::json geom = geometryOf(d.geometry);
            if (geom.is_null())
                continue;
            nlohmann::json props;
            props["id"] = d.id;
            props["disturbance_type"] = disturbanceTypeToString(d.type);
            props["zone"] = nullptr;       // C++ disturbances 模型未映射 zone 列
            props["area_m2"] = d.area;
            props["perimeter_m"] = d.perimeter;
            props["compliance"] = nullptr;  // C++ 模型无此字段
            props["in_boundary"] = nullptr; // C++ 模型无此字段
            props["confidence"] = d.confidence;
            props["period_id"] = d.period_id;
            features.push_back(utils::GeoJsonUtils::makeFeature(geom, props));
        }
        return utils::GeoJsonUtils::makeFeatureCollection(features);
    }

    nlohmann::json SceneLayerService::measures(int projectId, std::optional<int> periodId,
                                               const std::string& category)
    {
        std::vector<models::Measure> rows;
        if (periodId.has_value())
        {
            if (!periodBelongsToProject(projectId, periodId))
                return emptyFeatureCollection();
            rows = measureDao_->findByPeriodId(*periodId);
        }
        else
        {
            for (int pid : periodIdsForProject(projectId))
            {
                auto part = measureDao_->findByPeriodId(pid);
                rows.insert(rows.end(), part.begin(), part.end());
            }
        }

        // 对齐 Python: category 过滤 (engineering/vegetation/temporary)
        if (!category.empty())
        {
            rows.erase(std::remove_if(rows.begin(), rows.end(),
                                      [&category](const models::Measure& m)
                                      { return measureCategoryToString(m.type) != category; }),
                       rows.end());
        }

        // 对齐 Python: ORDER BY id DESC LIMIT 2000
        std::sort(rows.begin(), rows.end(),
                  [](const models::Measure& a, const models::Measure& b) { return a.id > b.id; });
        if (rows.size() > 2000)
            rows.resize(2000);

        nlohmann::json features = nlohmann::json::array();
        for (const auto& m : rows)
        {
            nlohmann::json geom = geometryOf(m.geometry);
            if (geom.is_null())
                continue;
            nlohmann::json props;
            props["id"] = m.id;
            props["category"] = measureCategoryToString(m.type);
            props["measure_type"] = m.subtype;
            props["name"] = nullptr;               // C++ measures 模型无 name 列
            props["zone"] = nullptr;               // C++ measures 模型未映射 zone 列
            props["length_m"] = m.length;
            props["area_m2"] = m.area;
            props["status"] = m.status;
            props["condition"] = m.condition;
            if (m.vegetation_coverage)
                props["vegetation_coverage"] = *m.vegetation_coverage;
            else
                props["vegetation_coverage"] = nullptr;
            props["integrity_rate"] = nullptr;      // C++ 模型无此字段
            props["implementation_rate"] = nullptr; // C++ 模型无此字段
            props["period_id"] = m.period_id;
            features.push_back(utils::GeoJsonUtils::makeFeature(geom, props));
        }
        return utils::GeoJsonUtils::makeFeatureCollection(features);
    }

    nlohmann::json SceneLayerService::anomalies(int projectId, std::optional<int> periodId)
    {
        // AnomalyDao 提供按项目查询 (detected_at DESC); DAO 模型含 project_id
        auto rows = anomalyDao_->findByProjectId(projectId);

        // 对齐 Python: 仅未处置异常 (排除已处置), period 过滤, LIMIT 500
        std::vector<models::Anomaly> filtered;
        for (const auto& a : rows)
        {
            if (a.status == models::DisposalStatus::Completed)
                continue;
            if (periodId.has_value() && a.period_id != *periodId)
                continue;
            filtered.push_back(a);
            if (filtered.size() >= 500)
                break;
        }

        nlohmann::json features = nlohmann::json::array();
        for (const auto& a : filtered)
        {
            nlohmann::json geom = geometryOf(a.geometry);
            if (geom.is_null())
                continue;
            nlohmann::json props;
            props["id"] = a.id;
            props["anomaly_type"] = anomalyTypeToString(a.type);
            props["severity"] = severityToString(a.severity);
            props["title"] = a.description; // C++ anomalies 模型无 title 列, 用 description 代替
            props["status"] = disposalStatusToString(a.status);
            props["area_m2"] = nullptr;     // C++ anomalies 模型无面积列
            features.push_back(utils::GeoJsonUtils::makeFeature(geom, props));
        }
        return utils::GeoJsonUtils::makeFeatureCollection(features);
    }

    nlohmann::json SceneLayerService::spoilDumps(int projectId, std::optional<int> periodId)
    {
        if (!spoilDao_)
            return emptyFeatureCollection();

        auto rows = spoilDao_->findByProject(projectId, periodId, "", "");

        nlohmann::json features = nlohmann::json::array();
        for (const auto& s : rows)
        {
            nlohmann::json geom = geometryOf(s.geom);
            if (geom.is_null())
                continue;
            nlohmann::json props;
            props["id"] = s.id;
            props["area_m2"] = s.area_m2;
            props["stability_level"] = s.stability_level;
            props["compliance"] = s.compliance;
            props["detect_method"] = s.detect_method;
            if (s.volume_m3)
                props["volume_m3"] = *s.volume_m3;
            else
                props["volume_m3"] = nullptr;
            if (s.max_height_m)
                props["max_height_m"] = *s.max_height_m;
            if (s.confidence)
                props["confidence"] = *s.confidence;
            props["period_id"] = s.period_id;
            features.push_back(utils::GeoJsonUtils::makeFeature(geom, props));
        }
        return utils::GeoJsonUtils::makeFeatureCollection(features);
    }

    nlohmann::json SceneLayerService::insarResults(int projectId)
    {
        // 对齐 Python: 列出 InSAR 形变成果 (terrain_results analysis=insar), 过滤文件已丢失记录
        nlohmann::json out = nlohmann::json::array();
        if (!terrainDao_)
            return out;

        auto rows = terrainDao_->listByProject(projectId, "insar");
        for (const auto& r : rows)
        {
            if (r.raster_path.empty() || !std::filesystem::exists(r.raster_path))
                continue;
            nlohmann::json j;
            j["id"] = r.id;
            j["raster_path"] = r.raster_path;
            if (!r.stats_json.empty())
            {
                auto stats = nlohmann::json::parse(r.stats_json, nullptr, false);
                if (!stats.is_discarded())
                    j["stats"] = std::move(stats);
            }
            j["created_at"] = isoTime(r.created_at);
            out.push_back(j);
        }
        return out;
    }

    nlohmann::json SceneLayerService::inundationGeoJson(int projectId, std::optional<int> resultId)
    {
        if (!terrainDao_ || !vectorProcessor_)
            return emptyFeatureCollection();

        models::TerrainResult row;
        bool found = false;
        if (resultId.has_value())
        {
            auto opt = terrainDao_->findById(*resultId);
            if (opt)
            {
                row = *opt;
                found = true;
            }
        }
        else
        {
            auto opt = terrainDao_->latestByProjectAndAnalysis(projectId, "inundation");
            if (opt)
            {
                row = *opt;
                found = true;
            }
        }
        if (!found || row.raster_path.empty() || !std::filesystem::exists(row.raster_path))
            return emptyFeatureCollection();

        nlohmann::json stats;
        if (!row.stats_json.empty())
            stats = nlohmann::json::parse(row.stats_json, nullptr, false);
        if (stats.is_discarded())
            stats = nlohmann::json::object();

        nlohmann::json features = nlohmann::json::array();
        for (const auto& geom : vectorProcessor_->rasterToVector(row.raster_path))
        {
            nlohmann::json gj = geometryOf(geom);
            if (gj.is_null())
                continue;
            nlohmann::json props;
            props["area_m2"] = stats.value("inundated_area_m2", nlohmann::json());
            if (stats.contains("depth_mean_m"))
                props["depth_mean_m"] = stats["depth_mean_m"];
            if (stats.contains("depth_max_m"))
                props["depth_max_m"] = stats["depth_max_m"];
            features.push_back(utils::GeoJsonUtils::makeFeature(gj, props));
        }
        return utils::GeoJsonUtils::makeFeatureCollection(features);
    }

    nlohmann::json SceneLayerService::viewshedGeoJson(int projectId, std::optional<int> resultId)
    {
        if (!terrainDao_ || !vectorProcessor_)
            return emptyFeatureCollection();

        models::TerrainResult row;
        bool found = false;
        if (resultId.has_value())
        {
            auto opt = terrainDao_->findById(*resultId);
            if (opt)
            {
                row = *opt;
                found = true;
            }
        }
        else
        {
            auto opt = terrainDao_->latestByProjectAndAnalysis(projectId, "viewshed");
            if (opt)
            {
                row = *opt;
                found = true;
            }
        }
        if (!found || row.raster_path.empty() || !std::filesystem::exists(row.raster_path))
            return emptyFeatureCollection();

        nlohmann::json stats;
        if (!row.stats_json.empty())
            stats = nlohmann::json::parse(row.stats_json, nullptr, false);
        if (stats.is_discarded())
            stats = nlohmann::json::object();

        nlohmann::json features = nlohmann::json::array();
        for (const auto& geom : vectorProcessor_->rasterToVector(row.raster_path))
        {
            nlohmann::json gj = geometryOf(geom);
            if (gj.is_null())
                continue;
            nlohmann::json props;
            props["visible_area_m2"] = stats.value("visible_area_m2", nlohmann::json());
            props["visible_ratio"] = stats.value("visible_ratio", nlohmann::json());
            features.push_back(utils::GeoJsonUtils::makeFeature(gj, props));
        }
        return utils::GeoJsonUtils::makeFeatureCollection(features);
    }

    nlohmann::json SceneLayerService::availability(int projectId, std::optional<int> periodId)
    {
        nlohmann::json rs;
        rs["fvc"] = false;
        rs["coverage"] = false;   // 无对应表
        rs["erosion"] = false;
        rs["landuse"] = false;    // 无对应表 (C++ 未注入 landuse DAO)
        int insarCount = 0;
        int inundationCount = 0;

        int disturbanceCount = 0;
        int measureCount = 0;
        int spoilCount = 0;

        if (periodId.has_value())
        {
            if (periodBelongsToProject(projectId, periodId))
            {
                disturbanceCount = static_cast<int>(disturbanceDao_->findByPeriodId(*periodId).size());
                measureCount = static_cast<int>(measureDao_->findByPeriodId(*periodId).size());
                rs["fvc"] = fvcDao_->findByPeriodId(*periodId).has_value();
                rs["erosion"] = erosionDao_->findByPeriodId(*periodId).has_value();
                if (spoilDao_)
                    spoilCount = static_cast<int>(spoilDao_->findByProject(projectId, periodId).size());
            }
        }
        else
        {
            for (int pid : periodIdsForProject(projectId))
            {
                disturbanceCount += static_cast<int>(disturbanceDao_->findByPeriodId(pid).size());
                measureCount += static_cast<int>(measureDao_->findByPeriodId(pid).size());
            }
            if (spoilDao_)
                spoilCount = static_cast<int>(spoilDao_->findByProject(projectId, periodId).size());
        }

        if (terrainDao_)
        {
            insarCount = static_cast<int>(terrainDao_->listByProject(projectId, "insar").size());
            inundationCount = static_cast<int>(terrainDao_->listByProject(projectId, "inundation").size());
        }
        rs["insar"] = insarCount;

        nlohmann::json vector;
        vector["disturbance"] = disturbanceCount;
        vector["measure"] = measureCount;
        vector["spoil_dump"] = spoilCount;

        nlohmann::json out;
        out["rs"] = rs;
        out["vector"] = vector;
        out["inundation"] = inundationCount;
        return out;
    }

}  // namespace soilwatercm::services
