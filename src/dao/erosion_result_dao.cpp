#include "erosion_result_dao.h"
namespace soilwatercm::dao
{
    ErosionResultDao::ErosionResultDao(std::shared_ptr<repository::PostgresRepository> repo)
        : DaoBase<models::ErosionResult>(repo)
    {
    }
    std::optional<models::ErosionResult> ErosionResultDao::findById(int id)
    {
        std::string sql = buildSelectQuery(
            "erosion_results",
            {"id", "modulus_raster_path", "class_raster_path", "mean_modulus", "slight_area", "light_area",
             "moderate_area", "strong_area", "very_strong_area", "severe_area", "period_id", "created_at"},
            "id = " + std::to_string(id));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRowToER(result[0]);
    }
    std::optional<models::ErosionResult> ErosionResultDao::findByPeriodId(int periodId)
    {
        std::string sql = buildSelectQuery(
            "erosion_results",
            {"id", "modulus_raster_path", "class_raster_path", "mean_modulus", "slight_area", "light_area",
             "moderate_area", "strong_area", "very_strong_area", "severe_area", "period_id", "created_at"},
            "period_id = " + std::to_string(periodId));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRowToER(result[0]);
    }
    std::vector<models::ErosionResult> ErosionResultDao::list(int page, int pageSize, const std::string& orderBy,
                                                              bool ascending)
    {
        std::string sql =
            buildSelectQuery("erosion_results", {"id", "modulus_raster_path", "class_raster_path", "mean_modulus",
                                                 "slight_area", "light_area", "moderate_area", "strong_area",
                                                 "very_strong_area", "severe_area", "period_id", "created_at"});
        sql += buildPagination(page, pageSize, orderBy, ascending);
        auto result = repo_->executeQuery(sql);
        std::vector<models::ErosionResult> ers;
        for (const auto& row : result)
            ers.push_back(mapRowToER(row));
        return ers;
    }
    int ErosionResultDao::insert(const models::ErosionResult& er)
    {
        std::string sql = buildInsertQuery(
            "erosion_results",
            {"modulus_raster_path", "class_raster_path", "mean_modulus", "slight_area", "light_area", "moderate_area",
             "strong_area", "very_strong_area", "severe_area", "period_id", "created_at"},
            {"'" + escapeString(er.modulus_raster_path) + "'", "'" + escapeString(er.class_raster_path) + "'",
             std::to_string(er.mean_modulus), std::to_string(er.slight_area), std::to_string(er.light_area),
             std::to_string(er.moderate_area), std::to_string(er.strong_area), std::to_string(er.very_strong_area),
             std::to_string(er.severe_area), std::to_string(er.period_id),
             "'" + timestampToString(er.created_at) + "'"});
        auto result = repo_->executeQuery(sql);
        return result.empty() ? -1 : std::stoi(result[0]["id"]);
    }
    bool ErosionResultDao::update(const models::ErosionResult& er)
    {
        std::string sql =
            buildUpdateQuery("erosion_results", {"modulus_raster_path", "class_raster_path", "mean_modulus"},
                             {"'" + escapeString(er.modulus_raster_path) + "'",
                              "'" + escapeString(er.class_raster_path) + "'", std::to_string(er.mean_modulus)},
                             "id = " + std::to_string(er.id));
        return repo_->executeUpdate(sql);
    }
    bool ErosionResultDao::remove(int id)
    {
        return repo_->executeUpdate(buildDeleteQuery("erosion_results", "id = " + std::to_string(id)));
    }
    int ErosionResultDao::count()
    {
        auto result = repo_->executeQuery(buildCountQuery("erosion_results"));
        return result.empty() ? 0 : std::stoi(result[0]["count"]);
    }
    models::ErosionResult ErosionResultDao::mapRowToER(const std::map<std::string, std::string>& row)
    {
        models::ErosionResult er;
        er.id = std::stoi(row.at("id"));
        er.modulus_raster_path = row.at("modulus_raster_path");
        er.class_raster_path = row.at("class_raster_path");
        er.mean_modulus = std::stod(row.at("mean_modulus"));
        er.slight_area = std::stod(row.at("slight_area"));
        er.light_area = std::stod(row.at("light_area"));
        er.moderate_area = std::stod(row.at("moderate_area"));
        er.strong_area = std::stod(row.at("strong_area"));
        er.very_strong_area = std::stod(row.at("very_strong_area"));
        er.severe_area = std::stod(row.at("severe_area"));
        er.period_id = std::stoi(row.at("period_id"));
        er.created_at = stringToTimestamp(row.at("created_at"));
        return er;
    }
}  // namespace soilwatercm::dao
