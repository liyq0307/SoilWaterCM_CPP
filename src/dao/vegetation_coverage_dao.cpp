#include "vegetation_coverage_dao.h"
namespace soilwatercm::dao
{
    VegetationCoverageDao::VegetationCoverageDao(std::shared_ptr<repository::PostgresRepository> repo)
        : DaoBase<models::VegetationCoverage>(repo)
    {
    }
    std::optional<models::VegetationCoverage> VegetationCoverageDao::findById(int id)
    {
        std::string sql =
            buildSelectQuery("vegetation_coverages",
                             {"id", "fvc_raster_path", "mean_fvc", "low_coverage_area", "medium_low_area",
                              "medium_area", "medium_high_area", "high_coverage_area", "period_id", "created_at"},
                             "id = " + std::to_string(id));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRowToVC(result[0]);
    }
    std::optional<models::VegetationCoverage> VegetationCoverageDao::findByPeriodId(int periodId)
    {
        std::string sql =
            buildSelectQuery("vegetation_coverages",
                             {"id", "fvc_raster_path", "mean_fvc", "low_coverage_area", "medium_low_area",
                              "medium_area", "medium_high_area", "high_coverage_area", "period_id", "created_at"},
                             "period_id = " + std::to_string(periodId));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRowToVC(result[0]);
    }
    std::vector<models::VegetationCoverage> VegetationCoverageDao::list(int page, int pageSize,
                                                                        const std::string& orderBy, bool ascending)
    {
        std::string sql =
            buildSelectQuery("vegetation_coverages",
                             {"id", "fvc_raster_path", "mean_fvc", "low_coverage_area", "medium_low_area",
                              "medium_area", "medium_high_area", "high_coverage_area", "period_id", "created_at"});
        sql += buildPagination(page, pageSize, orderBy, ascending);
        auto result = repo_->executeQuery(sql);
        std::vector<models::VegetationCoverage> vcs;
        for (const auto& row : result)
            vcs.push_back(mapRowToVC(row));
        return vcs;
    }
    int VegetationCoverageDao::insert(const models::VegetationCoverage& vc)
    {
        std::string sql = buildInsertQuery(
            "vegetation_coverages",
            {"fvc_raster_path", "mean_fvc", "low_coverage_area", "medium_low_area", "medium_area", "medium_high_area",
             "high_coverage_area", "period_id", "created_at"},
            {"'" + escapeString(vc.fvc_raster_path) + "'", std::to_string(vc.mean_fvc),
             std::to_string(vc.low_coverage_area), std::to_string(vc.medium_low_area), std::to_string(vc.medium_area),
             std::to_string(vc.medium_high_area), std::to_string(vc.high_coverage_area), std::to_string(vc.period_id),
             "'" + timestampToString(vc.created_at) + "'"});
        auto result = repo_->executeQuery(sql);
        return result.empty() ? -1 : std::stoi(result[0]["id"]);
    }
    bool VegetationCoverageDao::update(const models::VegetationCoverage& vc)
    {
        std::string sql = buildUpdateQuery("vegetation_coverages", {"fvc_raster_path", "mean_fvc"},
                                           {"'" + escapeString(vc.fvc_raster_path) + "'", std::to_string(vc.mean_fvc)},
                                           "id = " + std::to_string(vc.id));
        return repo_->executeUpdate(sql);
    }
    bool VegetationCoverageDao::remove(int id)
    {
        return repo_->executeUpdate(buildDeleteQuery("vegetation_coverages", "id = " + std::to_string(id)));
    }
    int VegetationCoverageDao::count()
    {
        auto result = repo_->executeQuery(buildCountQuery("vegetation_coverages"));
        return result.empty() ? 0 : std::stoi(result[0]["count"]);
    }
    models::VegetationCoverage VegetationCoverageDao::mapRowToVC(const std::map<std::string, std::string>& row)
    {
        models::VegetationCoverage vc;
        vc.id = std::stoi(row.at("id"));
        vc.fvc_raster_path = row.at("fvc_raster_path");
        vc.mean_fvc = std::stod(row.at("mean_fvc"));
        vc.low_coverage_area = std::stod(row.at("low_coverage_area"));
        vc.medium_low_area = std::stod(row.at("medium_low_area"));
        vc.medium_area = std::stod(row.at("medium_area"));
        vc.medium_high_area = std::stod(row.at("medium_high_area"));
        vc.high_coverage_area = std::stod(row.at("high_coverage_area"));
        vc.period_id = std::stoi(row.at("period_id"));
        vc.created_at = stringToTimestamp(row.at("created_at"));
        return vc;
    }
}  // namespace soilwatercm::dao
