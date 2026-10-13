#include "terrain_result_dao.h"

#include <optional>

namespace soilwatercm::dao
{
    static const std::vector<std::string> TERRAIN_FIELDS = {
        "id", "project_id", "analysis", "raster_path", "stats", "created_at"};

    TerrainResultDao::TerrainResultDao(std::shared_ptr<repository::PostgresRepository> repo)
        : DaoBase<models::TerrainResult>(repo)
    {
    }

    std::optional<models::TerrainResult> TerrainResultDao::findById(int id)
    {
        std::string sql = buildSelectQuery("terrain_results", TERRAIN_FIELDS, "id = " + std::to_string(id));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRow(result[0]);
    }

    std::vector<models::TerrainResult> TerrainResultDao::list(int page, int pageSize, const std::string& orderBy,
                                                              bool ascending)
    {
        std::string sql = buildSelectQuery("terrain_results", TERRAIN_FIELDS);
        sql += buildPagination(page, pageSize, orderBy, ascending);
        auto result = repo_->executeQuery(sql);
        std::vector<models::TerrainResult> rows;
        for (const auto& row : result)
            rows.push_back(mapRow(row));
        return rows;
    }

    int TerrainResultDao::insert(const models::TerrainResult& tr)
    {
        std::string sql = buildInsertQuery(
            "terrain_results",
            {"project_id", "analysis", "raster_path", "stats", "created_at"},
            {std::to_string(tr.project_id),
             "'" + escapeString(tr.analysis) + "'",
             tr.raster_path.empty() ? "NULL" : "'" + escapeString(tr.raster_path) + "'",
             tr.stats_json.empty() ? "NULL" : "'" + escapeString(tr.stats_json) + "'",
             "'" + timestampToString(tr.created_at) + "'"});
        auto result = repo_->executeQuery(sql);
        return result.empty() ? -1 : std::stoi(result[0]["id"]);
    }

    bool TerrainResultDao::update(const models::TerrainResult& tr)
    {
        std::string sql = buildUpdateQuery(
            "terrain_results",
            {"analysis", "raster_path", "stats"},
            {"'" + escapeString(tr.analysis) + "'",
             tr.raster_path.empty() ? "NULL" : "'" + escapeString(tr.raster_path) + "'",
             tr.stats_json.empty() ? "NULL" : "'" + escapeString(tr.stats_json) + "'"},
            "id = " + std::to_string(tr.id));
        return repo_->executeUpdate(sql);
    }

    bool TerrainResultDao::remove(int id)
    {
        return repo_->executeUpdate(buildDeleteQuery("terrain_results", "id = " + std::to_string(id)));
    }

    int TerrainResultDao::count()
    {
        auto result = repo_->executeQuery(buildCountQuery("terrain_results"));
        return result.empty() ? 0 : std::stoi(result[0]["count"]);
    }

    std::vector<models::TerrainResult> TerrainResultDao::listByProject(int projectId, const std::string& analysis)
    {
        std::string sql =
            buildSelectQuery("terrain_results", TERRAIN_FIELDS, "project_id = " + std::to_string(projectId));
        if (!analysis.empty())
            sql += " AND analysis = '" + escapeString(analysis) + "'";
        sql += " ORDER BY created_at DESC LIMIT 500";
        auto result = repo_->executeQuery(sql);
        std::vector<models::TerrainResult> rows;
        for (const auto& row : result)
            rows.push_back(mapRow(row));
        return rows;
    }

    std::optional<models::TerrainResult> TerrainResultDao::latestByProjectAndAnalysis(int projectId,
                                                                                     const std::string& analysis)
    {
        std::string sql = buildSelectQuery("terrain_results", TERRAIN_FIELDS,
                                           "project_id = " + std::to_string(projectId) +
                                           " AND analysis = '" + escapeString(analysis) + "'");
        sql += " ORDER BY created_at DESC LIMIT 1";
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRow(result[0]);
    }

    models::TerrainResult TerrainResultDao::mapRow(const std::map<std::string, std::string>& row)
    {
        models::TerrainResult tr;
        tr.id = std::stoi(row.at("id"));
        tr.project_id = std::stoi(row.at("project_id"));
        tr.analysis = row.at("analysis");
        tr.raster_path = row.at("raster_path");
        tr.stats_json = row.at("stats");
        tr.created_at = stringToTimestamp(row.at("created_at"));
        return tr;
    }
}  // namespace soilwatercm::dao
