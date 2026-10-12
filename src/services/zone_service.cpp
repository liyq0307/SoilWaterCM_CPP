#include "zone_service.h"

#include <algorithm>
#include <set>

#include "../common/exceptions.h"
#include "../utils/geojson_utils.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{

    namespace
    {
        std::string trimStr(const std::string& s)
        {
            size_t b = s.find_first_not_of(" \t\r\n");
            if (b == std::string::npos)
                return "";
            size_t e = s.find_last_not_of(" \t\r\n");
            return s.substr(b, e - b + 1);
        }

        std::string propAsString(const nlohmann::json& props, const std::vector<std::string>& keys)
        {
            for (const auto& k : keys)
            {
                if (props.contains(k) && props[k].is_string())
                {
                    std::string v = trimStr(props[k].get<std::string>());
                    if (!v.empty())
                        return v;
                }
            }
            return "";
        }
    }  // namespace

    std::optional<std::string> resolveZoneCode(const std::string& raw)
    {
        std::string s = trimStr(raw);
        if (s.empty())
            return std::nullopt;
        if (models::validZoneCodes().count(s))
            return s;
        auto it = models::zoneNameToCode().find(s);
        if (it != models::zoneNameToCode().end())
            return it->second;
        return std::nullopt;
    }

    ZoneService::ZoneService(std::shared_ptr<dao::ZoneDao> zoneDao) : zoneDao_(std::move(zoneDao)) {}

    std::vector<models::ZoneStats> ZoneService::listStats(int projectId, std::optional<int> periodId,
                                                          const std::string& zone)
    {
        return zoneDao_->findByProject(projectId, periodId, zone);
    }

    models::ZoneStats ZoneService::createStats(const models::ZoneStats& stats)
    {
        if (!models::validZoneCodes().count(stats.zone))
            throw common::ValidationException("无效分区代码: " + stats.zone);

        models::ZoneStats rec = stats;
        rec.created_at = std::chrono::system_clock::now();
        rec.updated_at = rec.created_at;
        int id = zoneDao_->insert(rec);
        if (id < 0)
            throw common::DatabaseException("创建分区统计失败");
        rec.id = id;
        return rec;
    }

    std::optional<models::ZoneStats> ZoneService::updateStats(int id, const nlohmann::json& patch)
    {
        auto existing = zoneDao_->findById(id);
        if (!existing)
            return std::nullopt;

        models::ZoneStats rec = *existing;
        if (patch.contains("period_id") && patch["period_id"].is_number())
            rec.period_id = patch["period_id"].get<int>();
        if (patch.contains("zone") && patch["zone"].is_string())
        {
            std::string z = patch["zone"].get<std::string>();
            if (!models::validZoneCodes().count(z))
                throw common::ValidationException("无效分区代码: " + z);
            rec.zone = z;
        }
        if (patch.contains("designed_area_ha"))
        {
            if (patch["designed_area_ha"].is_number())
                rec.designed_area_ha = patch["designed_area_ha"].get<double>();
            else if (patch["designed_area_ha"].is_null())
                rec.designed_area_ha = std::nullopt;
        }
        if (patch.contains("designed_measures") && patch["designed_measures"].is_string())
            rec.designed_measures = patch["designed_measures"].get<std::string>();
        if (patch.contains("progress_status") && patch["progress_status"].is_string())
            rec.progress_status = patch["progress_status"].get<std::string>();
        if (patch.contains("note") && patch["note"].is_string())
            rec.note = patch["note"].get<std::string>();

        rec.updated_at = std::chrono::system_clock::now();
        zoneDao_->update(rec);
        return zoneDao_->findById(id);
    }

    bool ZoneService::deleteStats(int id)
    {
        return zoneDao_->remove(id);
    }

    std::vector<models::ZoneBoundary> ZoneService::listBoundaries(int projectId)
    {
        return zoneDao_->listBoundaries(projectId);
    }

    nlohmann::json ZoneService::saveBoundaries(int projectId, const nlohmann::json& featureCollection)
    {
        nlohmann::json result;
        result["saved"] = nlohmann::json::array();
        result["skipped"] = nlohmann::json::array();

        if (!featureCollection.contains("features") || !featureCollection["features"].is_array() ||
            featureCollection["features"].empty())
            throw common::ValidationException("GeoJSON 无 features");

        // 覆盖式删除
        zoneDao_->deleteBoundariesByProject(projectId);

        std::set<std::string> saved;
        for (const auto& f : featureCollection["features"])
        {
            const auto& props =
                f.contains("properties") && f["properties"].is_object() ? f["properties"] : nlohmann::json::object();
            std::string raw = propAsString(props, {"zone", "Zone", "分区", "name", "Name"});
            auto code = resolveZoneCode(raw);
            if (!code)
            {
                result["skipped"].push_back(raw);
                continue;
            }
            if (!f.contains("geometry") || !f["geometry"].is_object())
            {
                result["skipped"].push_back(raw);
                continue;
            }
            std::string wkt = utils::GeoJsonUtils::geoJsonToWkt(f["geometry"]);
            if (wkt.empty())
            {
                result["skipped"].push_back(raw);
                continue;
            }

            models::ZoneBoundary b;
            b.project_id = projectId;
            b.zone = *code;
            b.geom.wkt = wkt;
            b.geom.srid = "EPSG:4326";
            double ha = utils::GeoJsonUtils::areaHa(wkt);
            if (ha >= 0)
                b.area_ha = ha;
            b.created_at = std::chrono::system_clock::now();
            zoneDao_->insertBoundary(b);
            result["saved"].push_back(*code);
        }

        return result;
    }

    nlohmann::json ZoneService::saveBoundariesFromFeatures(
        int projectId, const std::vector<std::pair<std::string, std::map<std::string, std::string>>>& features)
    {
        nlohmann::json result;
        result["saved"] = nlohmann::json::array();

        // 按 zone 代码分组收集多边形 (GeoJSON polygon 环列表)
        std::map<std::string, std::vector<nlohmann::json>> grouped;

        for (const auto& [wkt, attrs] : features)
        {
            if (wkt.empty())
                continue;

            std::string raw;
            for (const char* key : {"zone", "Zone", "分区", "组成部分", "code", "name", "Name"})
            {
                auto it = attrs.find(key);
                if (it != attrs.end() && !it->second.empty())
                {
                    raw = it->second;
                    break;
                }
            }
            auto code = resolveZoneCode(raw);
            if (!code)
                continue;

            nlohmann::json geom = utils::GeoJsonUtils::wktToGeoJson(wkt);
            if (geom.is_null())
                continue;
            std::string type = geom.value("type", "");
            if (type == "Polygon")
            {
                grouped[*code].push_back(geom["coordinates"]);
            }
            else if (type == "MultiPolygon")
            {
                for (const auto& poly : geom["coordinates"])
                    grouped[*code].push_back(poly);
            }
        }

        if (grouped.empty())
            throw common::ValidationException("文件中未找到有效分区几何");

        zoneDao_->deleteBoundariesByProject(projectId);

        for (const auto& [code, polys] : grouped)
        {
            nlohmann::json multi;
            multi["type"] = "MultiPolygon";
            multi["coordinates"] = polys;
            std::string mergedWkt = utils::GeoJsonUtils::geoJsonToWkt(multi);
            if (mergedWkt.empty())
                continue;

            models::ZoneBoundary b;
            b.project_id = projectId;
            b.zone = code;
            b.geom.wkt = mergedWkt;
            b.geom.srid = "EPSG:4326";
            double ha = utils::GeoJsonUtils::areaHa(mergedWkt);
            if (ha >= 0)
                b.area_ha = ha;
            b.created_at = std::chrono::system_clock::now();
            zoneDao_->insertBoundary(b);
            result["saved"].push_back(code);
        }

        return result;
    }

    nlohmann::json ZoneService::assignZones(int projectId, std::optional<int> periodId)
    {
        nlohmann::json result;
        auto boundaries = zoneDao_->listBoundaries(projectId);
        if (boundaries.empty())
        {
            result["zones"] = 0;
            result["disturbances_assigned"] = 0;
            result["measures_assigned"] = 0;
            result["unmatched"] = 0;
            result["detail"] = "无分区边界, 请先上传";
            return result;
        }

        int dOk = 0, dMiss = 0, mOk = 0, mMiss = 0;

        auto dGeoms = zoneDao_->listDisturbanceGeoms(projectId, periodId);
        for (const auto& [id, wkt] : dGeoms)
        {
            std::string z = zoneDao_->pickZoneForGeometry(projectId, wkt);
            if (!z.empty())
            {
                zoneDao_->setDisturbanceZone(id, z);
                ++dOk;
            }
            else
            {
                ++dMiss;
            }
        }

        auto mGeoms = zoneDao_->listMeasureGeoms(projectId, periodId);
        for (const auto& [id, wkt] : mGeoms)
        {
            std::string z = zoneDao_->pickZoneForGeometry(projectId, wkt);
            if (!z.empty())
            {
                zoneDao_->setMeasureZone(id, z);
                ++mOk;
            }
            else
            {
                ++mMiss;
            }
        }

        result["zones"] = static_cast<int>(boundaries.size());
        result["disturbances_assigned"] = dOk;
        result["measures_assigned"] = mOk;
        result["unmatched"] = dMiss + mMiss;
        return result;
    }

    nlohmann::json ZoneService::initZoneStats(int projectId, std::optional<int> periodId,
                                              const std::map<std::string, std::optional<double>>& zoneArea)
    {
        nlohmann::json result;
        int created = 0, skipped = 0;

        auto boundaries = zoneDao_->listBoundaries(projectId);
        std::map<std::string, std::optional<double>> boundaryArea;
        for (const auto& b : boundaries)
            boundaryArea[b.zone] = b.area_ha;

        auto pickArea = [&](const std::string& code) -> std::optional<double>
        {
            auto za = zoneArea.find(code);
            if (za != zoneArea.end() && za->second.has_value())
                return za->second;
            auto ba = boundaryArea.find(code);
            if (ba != boundaryArea.end())
                return ba->second;
            return std::nullopt;
        };

        for (const auto& [code, name] : models::zoneCodes())
        {
            auto existing = zoneDao_->findByProjectAndZone(projectId, code);
            if (existing)
            {
                if (!existing->designed_area_ha.has_value())
                {
                    auto area = pickArea(code);
                    if (area.has_value())
                        zoneDao_->updateDesignedArea(existing->id, *area);
                }
                ++skipped;
                continue;
            }

            models::ZoneStats s;
            s.project_id = projectId;
            s.period_id = periodId;
            s.zone = code;
            s.designed_area_ha = pickArea(code);
            s.created_at = std::chrono::system_clock::now();
            s.updated_at = s.created_at;
            zoneDao_->insert(s);
            ++created;
        }

        result["created"] = created;
        result["skipped"] = skipped;
        result["with_boundary"] = static_cast<int>(boundaryArea.size());
        return result;
    }

    nlohmann::json ZoneService::zoneCodesJson()
    {
        nlohmann::json j = nlohmann::json::object();
        for (const auto& [code, name] : models::zoneCodes())
            j[code] = name;
        return j;
    }

}  // namespace soilwatercm::services
