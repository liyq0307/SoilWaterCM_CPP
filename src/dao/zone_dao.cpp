#include "zone_dao.h"

#include <algorithm>

#include "utils/logger.h"

namespace soilwatercm::dao
{

    namespace
    {
        std::string optIntToSql(const std::optional<int>& v)
        {
            return v.has_value() ? std::to_string(*v) : "NULL";
        }

        std::string optDoubleToSql(const std::optional<double>& v)
        {
            return v.has_value() ? std::to_string(*v) : "NULL";
        }

        std::optional<double> parseOptDouble(const std::map<std::string, std::string>& row, const std::string& key)
        {
            auto it = row.find(key);
            if (it == row.end() || it->second.empty() || it->second == "NULL")
                return std::nullopt;
            try
            {
                return std::stod(it->second);
            }
            catch (const std::exception&)
            {
                return std::nullopt;
            }
        }

        std::string parseOptString(const std::map<std::string, std::string>& row, const std::string& key)
        {
            auto it = row.find(key);
            if (it == row.end() || it->second == "NULL")
                return "";
            return it->second;
        }
    }  // namespace

    ZoneDao::ZoneDao(std::shared_ptr<repository::PostgresRepository> repo) : DaoBase<models::ZoneStats>(repo) {}

    std::optional<models::ZoneStats> ZoneDao::findById(int id)
    {
        std::string sql = buildSelectQuery("zone_stats",
                                           {"id", "project_id", "period_id", "zone", "designed_area_ha",
                                            "designed_measures", "progress_status", "note", "created_at", "updated_at"},
                                           "id = " + std::to_string(id));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRowToStats(result[0]);
    }

    std::vector<models::ZoneStats> ZoneDao::list(int page, int pageSize, const std::string& orderBy, bool ascending)
    {
        std::string sql = buildSelectQuery(
            "zone_stats", {"id", "project_id", "period_id", "zone", "designed_area_ha", "designed_measures",
                           "progress_status", "note", "created_at", "updated_at"});
        sql += buildPagination(page, pageSize, orderBy, ascending);
        auto result = repo_->executeQuery(sql);
        std::vector<models::ZoneStats> rows;
        for (const auto& r : result)
            rows.push_back(mapRowToStats(r));
        return rows;
    }

    std::vector<models::ZoneStats> ZoneDao::findByProject(int projectId, std::optional<int> periodId,
                                                          const std::string& zone)
    {
        std::string where = "project_id = " + std::to_string(projectId);
        if (periodId.has_value())
            where += " AND period_id = " + std::to_string(*periodId);
        if (!zone.empty())
            where += " AND zone = '" + escapeString(zone) + "'";

        std::string sql = buildSelectQuery("zone_stats",
                                           {"id", "project_id", "period_id", "zone", "designed_area_ha",
                                            "designed_measures", "progress_status", "note", "created_at", "updated_at"},
                                           where);
        sql += " ORDER BY zone LIMIT 50";
        auto result = repo_->executeQuery(sql);
        std::vector<models::ZoneStats> rows;
        for (const auto& r : result)
            rows.push_back(mapRowToStats(r));
        return rows;
    }

    std::optional<models::ZoneStats> ZoneDao::findByProjectAndZone(int projectId, const std::string& zone)
    {
        std::string where = "project_id = " + std::to_string(projectId) + " AND zone = '" + escapeString(zone) + "'";
        std::string sql = buildSelectQuery("zone_stats",
                                           {"id", "project_id", "period_id", "zone", "designed_area_ha",
                                            "designed_measures", "progress_status", "note", "created_at", "updated_at"},
                                           where) +
                          " LIMIT 1";
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRowToStats(result[0]);
    }

    int ZoneDao::insert(const models::ZoneStats& stats)
    {
        std::string sql =
            "INSERT INTO zone_stats (project_id, period_id, zone, designed_area_ha, designed_measures, "
            "progress_status, note, created_at, updated_at) VALUES (" +
            std::to_string(stats.project_id) + ", " + optIntToSql(stats.period_id) + ", '" + escapeString(stats.zone) +
            "', " + optDoubleToSql(stats.designed_area_ha) + ", " +
            (stats.designed_measures.empty() ? "NULL" : "'" + escapeString(stats.designed_measures) + "'") + ", " +
            (stats.progress_status.empty() ? "NULL" : "'" + escapeString(stats.progress_status) + "'") + ", " +
            (stats.note.empty() ? "NULL" : "'" + escapeString(stats.note) + "'") + ", '" +
            timestampToString(stats.created_at) + "', '" + timestampToString(stats.updated_at) + "') RETURNING id";
        auto result = repo_->executeQuery(sql);
        return result.empty() ? -1 : std::stoi(result[0]["id"]);
    }

    bool ZoneDao::update(const models::ZoneStats& stats)
    {
        std::string sql =
            "UPDATE zone_stats SET period_id = " + optIntToSql(stats.period_id) + ", zone = '" +
            escapeString(stats.zone) + "', designed_area_ha = " + optDoubleToSql(stats.designed_area_ha) +
            ", designed_measures = " +
            (stats.designed_measures.empty() ? "NULL" : "'" + escapeString(stats.designed_measures) + "'") +
            ", progress_status = " +
            (stats.progress_status.empty() ? "NULL" : "'" + escapeString(stats.progress_status) + "'") +
            ", note = " + (stats.note.empty() ? "NULL" : "'" + escapeString(stats.note) + "'") + ", updated_at = '" +
            timestampToString(stats.updated_at) + "' WHERE id = " + std::to_string(stats.id);
        return repo_->executeUpdate(sql);
    }

    bool ZoneDao::updateDesignedArea(int id, double areaHa)
    {
        std::string sql = "UPDATE zone_stats SET designed_area_ha = " + std::to_string(areaHa) +
                          ", updated_at = now() WHERE id = " + std::to_string(id);
        return repo_->executeUpdate(sql);
    }

    bool ZoneDao::remove(int id)
    {
        std::string sql = buildDeleteQuery("zone_stats", "id = " + std::to_string(id));
        return repo_->executeUpdate(sql);
    }

    int ZoneDao::count()
    {
        std::string sql = buildCountQuery("zone_stats");
        auto result = repo_->executeQuery(sql);
        return result.empty() ? 0 : std::stoi(result[0]["count"]);
    }

    std::vector<models::ZoneBoundary> ZoneDao::listBoundaries(int projectId)
    {
        std::string sql = buildSelectQuery(
            "zone_boundaries", {"id", "project_id", "zone", "ST_AsText(geom) as geom", "area_ha", "note", "created_at"},
            "project_id = " + std::to_string(projectId));
        sql += " ORDER BY zone";
        auto result = repo_->executeQuery(sql);
        std::vector<models::ZoneBoundary> rows;
        for (const auto& r : result)
            rows.push_back(mapRowToBoundary(r));
        return rows;
    }

    int ZoneDao::insertBoundary(const models::ZoneBoundary& boundary)
    {
        std::string sql = "INSERT INTO zone_boundaries (project_id, zone, geom, area_ha, note, created_at) VALUES (" +
                          std::to_string(boundary.project_id) + ", '" + escapeString(boundary.zone) +
                          "', ST_GeomFromText('" + escapeString(boundary.geom.wkt) + "', 4326), " +
                          optDoubleToSql(boundary.area_ha) + ", " +
                          (boundary.note.empty() ? "NULL" : "'" + escapeString(boundary.note) + "'") + ", '" +
                          timestampToString(boundary.created_at) + "') RETURNING id";
        auto result = repo_->executeQuery(sql);
        return result.empty() ? -1 : std::stoi(result[0]["id"]);
    }

    bool ZoneDao::deleteBoundariesByProject(int projectId)
    {
        std::string sql = buildDeleteQuery("zone_boundaries", "project_id = " + std::to_string(projectId));
        return repo_->executeUpdate(sql);
    }

    std::string ZoneDao::pickZoneForGeometry(int projectId, const std::string& geomWkt)
    {
        if (geomWkt.empty())
            return "";

        std::string g = "ST_GeomFromText('" + escapeString(geomWkt) + "', 4326)";
        std::string sql = "SELECT zone FROM zone_boundaries WHERE project_id = " + std::to_string(projectId) +
                          " ORDER BY CASE WHEN ST_Contains(geom, ST_Centroid(" + g +
                          ")) THEN 0 ELSE 1 END, "
                          "CASE WHEN ST_Intersects(geom, " +
                          g + ") THEN ST_Area(ST_Intersection(geom, " + g + ")) ELSE 0 END DESC LIMIT 1";
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return "";
        return result[0]["zone"];
    }

    std::vector<std::pair<int, std::string>> ZoneDao::listDisturbanceGeoms(int projectId, std::optional<int> periodId)
    {
        std::string where = "p.project_id = " + std::to_string(projectId);
        if (periodId.has_value())
            where += " AND d.period_id = " + std::to_string(*periodId);

        std::string sql =
            "SELECT d.id, ST_AsText(d.geometry) AS geom FROM disturbances d "
            "JOIN monitoring_periods p ON d.period_id = p.id WHERE " +
            where;
        auto result = repo_->executeQuery(sql);
        std::vector<std::pair<int, std::string>> rows;
        for (const auto& r : result)
        {
            auto it = r.find("geom");
            if (it == r.end() || it->second.empty() || it->second == "NULL")
                continue;
            rows.emplace_back(std::stoi(r.at("id")), it->second);
        }
        return rows;
    }

    std::vector<std::pair<int, std::string>> ZoneDao::listMeasureGeoms(int projectId, std::optional<int> periodId)
    {
        std::string where = "p.project_id = " + std::to_string(projectId);
        if (periodId.has_value())
            where += " AND m.period_id = " + std::to_string(*periodId);

        std::string sql =
            "SELECT m.id, ST_AsText(m.geometry) AS geom FROM measures m "
            "JOIN monitoring_periods p ON m.period_id = p.id WHERE " +
            where;
        auto result = repo_->executeQuery(sql);
        std::vector<std::pair<int, std::string>> rows;
        for (const auto& r : result)
        {
            auto it = r.find("geom");
            if (it == r.end() || it->second.empty() || it->second == "NULL")
                continue;
            rows.emplace_back(std::stoi(r.at("id")), it->second);
        }
        return rows;
    }

    bool ZoneDao::setDisturbanceZone(int id, const std::string& zone)
    {
        std::string sql =
            "UPDATE disturbances SET zone = '" + escapeString(zone) + "' WHERE id = " + std::to_string(id);
        return repo_->executeUpdate(sql);
    }

    bool ZoneDao::setMeasureZone(int id, const std::string& zone)
    {
        std::string sql = "UPDATE measures SET zone = '" + escapeString(zone) + "' WHERE id = " + std::to_string(id);
        return repo_->executeUpdate(sql);
    }

    models::ZoneStats ZoneDao::mapRowToStats(const std::map<std::string, std::string>& row)
    {
        models::ZoneStats s;
        s.id = std::stoi(row.at("id"));
        s.project_id = std::stoi(row.at("project_id"));
        auto pidIt = row.find("period_id");
        if (pidIt != row.end() && !pidIt->second.empty() && pidIt->second != "NULL")
            s.period_id = std::stoi(pidIt->second);
        s.zone = row.at("zone");
        s.designed_area_ha = parseOptDouble(row, "designed_area_ha");
        s.designed_measures = parseOptString(row, "designed_measures");
        s.progress_status = parseOptString(row, "progress_status");
        s.note = parseOptString(row, "note");
        s.created_at = stringToTimestamp(row.at("created_at"));
        s.updated_at = stringToTimestamp(row.at("updated_at"));
        return s;
    }

    models::ZoneBoundary ZoneDao::mapRowToBoundary(const std::map<std::string, std::string>& row)
    {
        models::ZoneBoundary b;
        b.id = std::stoi(row.at("id"));
        b.project_id = std::stoi(row.at("project_id"));
        b.zone = row.at("zone");
        b.geom = wktToGeometry(row.at("geom"));
        b.area_ha = parseOptDouble(row, "area_ha");
        b.note = parseOptString(row, "note");
        b.created_at = stringToTimestamp(row.at("created_at"));
        return b;
    }

}  // namespace soilwatercm::dao
