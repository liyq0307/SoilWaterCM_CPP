#include "result_dao.h"

#include <optional>

namespace soilwatercm::dao
{
    // ================= LandUseResultDao =================

    static const std::vector<std::string> LANDUSE_FIELDS = {
        "id", "project_id", "period_id", "image_id", "method", "n_classes", "raster_path", "class_areas", "created_at"};

    LandUseResultDao::LandUseResultDao(std::shared_ptr<repository::PostgresRepository> repo)
        : DaoBase<models::LandUseResult>(repo)
    {
    }

    std::optional<models::LandUseResult> LandUseResultDao::findById(int id)
    {
        std::string sql = buildSelectQuery("landuse_results", LANDUSE_FIELDS, "id = " + std::to_string(id));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRow(result[0]);
    }

    std::vector<models::LandUseResult> LandUseResultDao::list(int page, int pageSize, const std::string& orderBy,
                                                               bool ascending)
    {
        std::string sql = buildSelectQuery("landuse_results", LANDUSE_FIELDS);
        sql += buildPagination(page, pageSize, orderBy, ascending);
        auto result = repo_->executeQuery(sql);
        std::vector<models::LandUseResult> rows;
        for (const auto& row : result)
            rows.push_back(mapRow(row));
        return rows;
    }

    int LandUseResultDao::insert(const models::LandUseResult& lu)
    {
        std::string sql = buildInsertQuery(
            "landuse_results",
            {"project_id", "period_id", "image_id", "method", "n_classes", "raster_path", "class_areas", "created_at"},
            {std::to_string(lu.project_id),
             lu.period_id ? std::to_string(*lu.period_id) : "NULL",
             lu.image_id ? std::to_string(*lu.image_id) : "NULL",
             "'" + escapeString(lu.method) + "'",
             lu.n_classes ? std::to_string(*lu.n_classes) : "NULL",
             "'" + escapeString(lu.raster_path) + "'",
             lu.class_areas_json.empty() ? "NULL" : "'" + escapeString(lu.class_areas_json) + "'",
             "'" + timestampToString(lu.created_at) + "'"});
        auto result = repo_->executeQuery(sql);
        return result.empty() ? -1 : std::stoi(result[0]["id"]);
    }

    bool LandUseResultDao::update(const models::LandUseResult& lu)
    {
        std::string sql =
            buildUpdateQuery("landuse_results", {"method", "n_classes", "raster_path", "class_areas"},
                             {"'" + escapeString(lu.method) + "'",
                              lu.n_classes ? std::to_string(*lu.n_classes) : "NULL",
                              "'" + escapeString(lu.raster_path) + "'",
                              lu.class_areas_json.empty() ? "NULL" : "'" + escapeString(lu.class_areas_json) + "'"},
                             "id = " + std::to_string(lu.id));
        return repo_->executeUpdate(sql);
    }

    bool LandUseResultDao::remove(int id)
    {
        return repo_->executeUpdate(buildDeleteQuery("landuse_results", "id = " + std::to_string(id)));
    }

    int LandUseResultDao::count()
    {
        auto result = repo_->executeQuery(buildCountQuery("landuse_results"));
        return result.empty() ? 0 : std::stoi(result[0]["count"]);
    }

    std::vector<models::LandUseResult> LandUseResultDao::findByProject(int projectId, std::optional<int> periodId)
    {
        std::string sql = buildSelectQuery("landuse_results", LANDUSE_FIELDS, "project_id = " + std::to_string(projectId));
        if (periodId.has_value())
            sql += " AND period_id = " + std::to_string(*periodId);
        sql += " ORDER BY created_at DESC LIMIT 500";
        auto result = repo_->executeQuery(sql);
        std::vector<models::LandUseResult> rows;
        for (const auto& row : result)
            rows.push_back(mapRow(row));
        return rows;
    }

    models::LandUseResult LandUseResultDao::mapRow(const std::map<std::string, std::string>& row)
    {
        models::LandUseResult lu;
        lu.id = std::stoi(row.at("id"));
        lu.project_id = std::stoi(row.at("project_id"));
        auto periodIt = row.find("period_id");
        if (periodIt != row.end() && !periodIt->second.empty())
            lu.period_id = std::stoi(periodIt->second);
        auto imageIt = row.find("image_id");
        if (imageIt != row.end() && !imageIt->second.empty())
            lu.image_id = std::stoi(imageIt->second);
        lu.method = row.at("method");
        auto classesIt = row.find("n_classes");
        if (classesIt != row.end() && !classesIt->second.empty())
            lu.n_classes = std::stoi(classesIt->second);
        lu.raster_path = row.at("raster_path");
        auto areasIt = row.find("class_areas");
        if (areasIt != row.end())
            lu.class_areas_json = areasIt->second;
        lu.created_at = stringToTimestamp(row.at("created_at"));
        return lu;
    }

    // ================= SegmentationResultDao =================

    static const std::vector<std::string> SEGMENTATION_FIELDS = {
        "id", "project_id", "period_id", "image_id", "method", "n_segments", "raster_path", "created_at"};

    SegmentationResultDao::SegmentationResultDao(std::shared_ptr<repository::PostgresRepository> repo)
        : DaoBase<models::SegmentationResult>(repo)
    {
    }

    std::optional<models::SegmentationResult> SegmentationResultDao::findById(int id)
    {
        std::string sql = buildSelectQuery("segmentation_results", SEGMENTATION_FIELDS, "id = " + std::to_string(id));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRow(result[0]);
    }

    std::vector<models::SegmentationResult> SegmentationResultDao::list(int page, int pageSize,
                                                                         const std::string& orderBy, bool ascending)
    {
        std::string sql = buildSelectQuery("segmentation_results", SEGMENTATION_FIELDS);
        sql += buildPagination(page, pageSize, orderBy, ascending);
        auto result = repo_->executeQuery(sql);
        std::vector<models::SegmentationResult> rows;
        for (const auto& row : result)
            rows.push_back(mapRow(row));
        return rows;
    }

    int SegmentationResultDao::insert(const models::SegmentationResult& seg)
    {
        std::string sql = buildInsertQuery(
            "segmentation_results",
            {"project_id", "period_id", "image_id", "method", "n_segments", "raster_path", "created_at"},
            {seg.project_id ? std::to_string(*seg.project_id) : "NULL",
             seg.period_id ? std::to_string(*seg.period_id) : "NULL",
             std::to_string(seg.image_id),
             "'" + escapeString(seg.method) + "'",
             seg.n_segments ? std::to_string(*seg.n_segments) : "NULL",
             "'" + escapeString(seg.raster_path) + "'",
             "'" + timestampToString(seg.created_at) + "'"});
        auto result = repo_->executeQuery(sql);
        return result.empty() ? -1 : std::stoi(result[0]["id"]);
    }

    bool SegmentationResultDao::update(const models::SegmentationResult& seg)
    {
        std::string sql =
            buildUpdateQuery("segmentation_results", {"method", "n_segments", "raster_path"},
                             {"'" + escapeString(seg.method) + "'",
                              seg.n_segments ? std::to_string(*seg.n_segments) : "NULL",
                              "'" + escapeString(seg.raster_path) + "'"},
                             "id = " + std::to_string(seg.id));
        return repo_->executeUpdate(sql);
    }

    bool SegmentationResultDao::remove(int id)
    {
        return repo_->executeUpdate(buildDeleteQuery("segmentation_results", "id = " + std::to_string(id)));
    }

    int SegmentationResultDao::count()
    {
        auto result = repo_->executeQuery(buildCountQuery("segmentation_results"));
        return result.empty() ? 0 : std::stoi(result[0]["count"]);
    }

    std::vector<models::SegmentationResult> SegmentationResultDao::findByFilter(std::optional<int> imageId,
                                                                                std::optional<int> projectId)
    {
        std::vector<std::string> whereParts;
        if (imageId.has_value())
            whereParts.push_back("image_id = " + std::to_string(*imageId));
        if (projectId.has_value())
            whereParts.push_back("project_id = " + std::to_string(*projectId));
        std::string sql = buildSelectQuery("segmentation_results", SEGMENTATION_FIELDS);
        if (!whereParts.empty())
        {
            sql += " WHERE ";
            for (size_t i = 0; i < whereParts.size(); ++i)
            {
                if (i > 0)
                    sql += " AND ";
                sql += whereParts[i];
            }
        }
        sql += " ORDER BY created_at DESC LIMIT 500";
        auto result = repo_->executeQuery(sql);
        std::vector<models::SegmentationResult> rows;
        for (const auto& row : result)
            rows.push_back(mapRow(row));
        return rows;
    }

    models::SegmentationResult SegmentationResultDao::mapRow(const std::map<std::string, std::string>& row)
    {
        models::SegmentationResult seg;
        seg.id = std::stoi(row.at("id"));
        auto projectIt = row.find("project_id");
        if (projectIt != row.end() && !projectIt->second.empty())
            seg.project_id = std::stoi(projectIt->second);
        auto periodIt = row.find("period_id");
        if (periodIt != row.end() && !periodIt->second.empty())
            seg.period_id = std::stoi(periodIt->second);
        seg.image_id = std::stoi(row.at("image_id"));
        seg.method = row.at("method");
        auto segmentsIt = row.find("n_segments");
        if (segmentsIt != row.end() && !segmentsIt->second.empty())
            seg.n_segments = std::stoi(segmentsIt->second);
        seg.raster_path = row.at("raster_path");
        seg.created_at = stringToTimestamp(row.at("created_at"));
        return seg;
    }
}  // namespace soilwatercm::dao
