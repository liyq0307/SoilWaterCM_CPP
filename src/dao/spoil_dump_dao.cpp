#include "spoil_dump_dao.h"

#include <optional>

namespace soilwatercm::dao
{
    static const std::vector<std::string> SPOIL_FIELDS = {
        "id",   "project_id", "period_id", "image_id", "ST_AsText(geom) as geom", "area_m2",
        "volume_m3", "max_height_m", "mean_height_m", "mean_slope_deg", "in_approved_area",
        "has_protection", "protection_types", "stability_level", "risk_score", "compliance",
        "detect_method", "confidence", "created_at"};

    SpoilDumpDao::SpoilDumpDao(std::shared_ptr<repository::PostgresRepository> repo)
        : DaoBase<models::SpoilDump>(repo)
    {
    }

    std::optional<models::SpoilDump> SpoilDumpDao::findById(int id)
    {
        std::string sql = buildSelectQuery("spoil_dumps", SPOIL_FIELDS, "id = " + std::to_string(id));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRow(result[0]);
    }

    std::vector<models::SpoilDump> SpoilDumpDao::list(int page, int pageSize, const std::string& orderBy,
                                                       bool ascending)
    {
        std::string sql = buildSelectQuery("spoil_dumps", SPOIL_FIELDS);
        sql += buildPagination(page, pageSize, orderBy, ascending);
        auto result = repo_->executeQuery(sql);
        std::vector<models::SpoilDump> rows;
        for (const auto& row : result)
            rows.push_back(mapRow(row));
        return rows;
    }

    int SpoilDumpDao::insert(const models::SpoilDump& sd)
    {
        std::string sql = buildInsertQuery(
            "spoil_dumps",
            {"project_id", "period_id", "image_id", "geom", "area_m2", "volume_m3", "max_height_m", "mean_height_m",
             "mean_slope_deg", "in_approved_area", "has_protection", "protection_types", "stability_level",
             "risk_score", "compliance", "detect_method", "confidence", "created_at"},
            {std::to_string(sd.project_id),
             std::to_string(sd.period_id),
             sd.image_id ? std::to_string(*sd.image_id) : "NULL",
             "ST_GeomFromText('" + escapeString(sd.geom.wkt) + "', 4326)",
             std::to_string(sd.area_m2),
             sd.volume_m3 ? std::to_string(*sd.volume_m3) : "NULL",
             sd.max_height_m ? std::to_string(*sd.max_height_m) : "NULL",
             sd.mean_height_m ? std::to_string(*sd.mean_height_m) : "NULL",
             sd.mean_slope_deg ? std::to_string(*sd.mean_slope_deg) : "NULL",
             sd.in_approved_area ? (*sd.in_approved_area ? "TRUE" : "FALSE") : "NULL",
             sd.has_protection ? (*sd.has_protection ? "TRUE" : "FALSE") : "NULL",
             "'" + escapeString(sd.protection_types) + "'",
             "'" + escapeString(sd.stability_level) + "'",
             sd.risk_score ? std::to_string(*sd.risk_score) : "NULL",
             "'" + escapeString(sd.compliance) + "'",
             "'" + escapeString(sd.detect_method) + "'",
             sd.confidence ? std::to_string(*sd.confidence) : "NULL",
             "'" + timestampToString(sd.created_at) + "'"});
        auto result = repo_->executeQuery(sql);
        return result.empty() ? -1 : std::stoi(result[0]["id"]);
    }

    bool SpoilDumpDao::update(const models::SpoilDump& sd)
    {
        std::string sql = buildUpdateQuery(
            "spoil_dumps",
            {"stability_level", "risk_score", "compliance", "confidence"},
            {"'" + escapeString(sd.stability_level) + "'",
             sd.risk_score ? std::to_string(*sd.risk_score) : "NULL",
             "'" + escapeString(sd.compliance) + "'",
             sd.confidence ? std::to_string(*sd.confidence) : "NULL"},
            "id = " + std::to_string(sd.id));
        return repo_->executeUpdate(sql);
    }

    bool SpoilDumpDao::remove(int id)
    {
        return repo_->executeUpdate(buildDeleteQuery("spoil_dumps", "id = " + std::to_string(id)));
    }

    int SpoilDumpDao::count()
    {
        auto result = repo_->executeQuery(buildCountQuery("spoil_dumps"));
        return result.empty() ? 0 : std::stoi(result[0]["count"]);
    }

    std::vector<models::SpoilDump> SpoilDumpDao::findByProject(int projectId, std::optional<int> periodId,
                                                                const std::string& compliance,
                                                                const std::string& stabilityLevel)
    {
        std::string sql =
            buildSelectQuery("spoil_dumps", SPOIL_FIELDS, "project_id = " + std::to_string(projectId));
        if (periodId.has_value())
            sql += " AND period_id = " + std::to_string(*periodId);
        if (!compliance.empty())
            sql += " AND compliance = '" + escapeString(compliance) + "'";
        if (!stabilityLevel.empty())
            sql += " AND stability_level = '" + escapeString(stabilityLevel) + "'";
        sql += " ORDER BY volume_m3 DESC NULLS LAST LIMIT 2000";
        auto result = repo_->executeQuery(sql);
        std::vector<models::SpoilDump> rows;
        for (const auto& row : result)
            rows.push_back(mapRow(row));
        return rows;
    }

    models::SpoilDump SpoilDumpDao::mapRow(const std::map<std::string, std::string>& row)
    {
        models::SpoilDump sd;
        sd.id = std::stoi(row.at("id"));
        sd.project_id = std::stoi(row.at("project_id"));
        sd.period_id = std::stoi(row.at("period_id"));
        auto imageIt = row.find("image_id");
        if (imageIt != row.end() && !imageIt->second.empty())
            sd.image_id = std::stoi(imageIt->second);
        auto geomIt = row.find("geom");
        if (geomIt != row.end() && !geomIt->second.empty())
            sd.geom = wktToGeometry(geomIt->second);
        sd.area_m2 = std::stod(row.at("area_m2"));

        auto optDbl = [&](const std::string& key) -> std::optional<double>
        {
            auto it = row.find(key);
            if (it != row.end() && !it->second.empty())
                return std::stod(it->second);
            return std::nullopt;
        };
        auto optBool = [&](const std::string& key) -> std::optional<bool>
        {
            auto it = row.find(key);
            if (it != row.end() && !it->second.empty())
                return it->second == "t" || it->second == "true" || it->second == "TRUE";
            return std::nullopt;
        };
        sd.volume_m3 = optDbl("volume_m3");
        sd.max_height_m = optDbl("max_height_m");
        sd.mean_height_m = optDbl("mean_height_m");
        sd.mean_slope_deg = optDbl("mean_slope_deg");
        sd.in_approved_area = optBool("in_approved_area");
        sd.has_protection = optBool("has_protection");
        sd.protection_types = row.count("protection_types") ? row.at("protection_types") : "";
        sd.stability_level = row.at("stability_level");
        sd.risk_score = optDbl("risk_score");
        sd.compliance = row.at("compliance");
        sd.detect_method = row.at("detect_method");
        sd.confidence = optDbl("confidence");
        sd.created_at = stringToTimestamp(row.at("created_at"));
        return sd;
    }
}  // namespace soilwatercm::dao
