#include "measure_dao.h"
namespace soilwatercm::dao
{
    MeasureDao::MeasureDao(std::shared_ptr<repository::PostgresRepository> repo) : DaoBase<models::Measure>(repo) {}
    std::optional<models::Measure> MeasureDao::findById(int id)
    {
        std::string sql =
            buildSelectQuery("measures",
                             {"id", "type", "subtype", "ST_AsText(geometry) as geometry", "length", "area", "status",
                              "condition", "vegetation_coverage", "period_id", "created_at"},
                             "id = " + std::to_string(id));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRowToMeasure(result[0]);
    }
    std::vector<models::Measure> MeasureDao::findByPeriodId(int periodId)
    {
        std::string sql =
            buildSelectQuery("measures",
                             {"id", "type", "subtype", "ST_AsText(geometry) as geometry", "length", "area", "status",
                              "condition", "vegetation_coverage", "period_id", "created_at"},
                             "period_id = " + std::to_string(periodId));
        auto result = repo_->executeQuery(sql);
        std::vector<models::Measure> measures;
        for (const auto& row : result)
            measures.push_back(mapRowToMeasure(row));
        return measures;
    }
    std::vector<models::Measure> MeasureDao::findByProject(int projectId, std::optional<int> periodId)
    {
        std::string where = "mp.project_id = " + std::to_string(projectId);
        if (periodId.has_value())
            where += " AND m.period_id = " + std::to_string(*periodId);

        std::string sql =
            buildSelectQuery("measures m JOIN monitoring_periods mp ON m.period_id = mp.id",
                             {"m.id", "m.type", "m.subtype", "ST_AsText(m.geometry) as geometry", "m.length", "m.area",
                              "m.status", "m.condition", "m.vegetation_coverage", "m.period_id", "m.created_at"},
                             where);
        sql += " ORDER BY m.id DESC LIMIT 2000";
        auto result = repo_->executeQuery(sql);
        std::vector<models::Measure> measures;
        for (const auto& row : result)
            measures.push_back(mapRowToMeasure(row));
        return measures;
    }
    std::vector<models::Measure> MeasureDao::list(int page, int pageSize, const std::string& orderBy, bool ascending)
    {
        std::string sql =
            buildSelectQuery("measures", {"id", "type", "subtype", "ST_AsText(geometry) as geometry", "length", "area",
                                          "status", "condition", "vegetation_coverage", "period_id", "created_at"});
        sql += buildPagination(page, pageSize, orderBy, ascending);
        auto result = repo_->executeQuery(sql);
        std::vector<models::Measure> measures;
        for (const auto& row : result)
            measures.push_back(mapRowToMeasure(row));
        return measures;
    }
    int MeasureDao::insert(const models::Measure& m)
    {
        std::string sql =
            "INSERT INTO measures (type, subtype, geometry, length, area, status, condition, vegetation_coverage, "
            "period_id, created_at) VALUES (" +
            std::to_string(static_cast<int>(m.type)) + ", '" + escapeString(m.subtype) + "', " + "ST_GeomFromText('" +
            escapeString(m.geometry.wkt) + "', 4326), " + std::to_string(m.length) + ", " + std::to_string(m.area) +
            ", '" + escapeString(m.status) + "', '" + escapeString(m.condition) + "', " +
            (m.vegetation_coverage ? std::to_string(*m.vegetation_coverage) : "NULL") + ", " +
            std::to_string(m.period_id) + ", '" + timestampToString(m.created_at) + "') RETURNING id";
        auto result = repo_->executeQuery(sql);
        return result.empty() ? -1 : std::stoi(result[0]["id"]);
    }
    bool MeasureDao::update(const models::Measure& m)
    {
        std::string sql = buildUpdateQuery("measures", {"status", "condition"},
                                           {"'" + escapeString(m.status) + "'", "'" + escapeString(m.condition) + "'"},
                                           "id = " + std::to_string(m.id));
        return repo_->executeUpdate(sql);
    }
    bool MeasureDao::remove(int id)
    {
        return repo_->executeUpdate(buildDeleteQuery("measures", "id = " + std::to_string(id)));
    }
    int MeasureDao::count()
    {
        auto result = repo_->executeQuery(buildCountQuery("measures"));
        return result.empty() ? 0 : std::stoi(result[0]["count"]);
    }
    models::Measure MeasureDao::mapRowToMeasure(const std::map<std::string, std::string>& row)
    {
        models::Measure m;
        m.id = std::stoi(row.at("id"));
        m.type = static_cast<models::MeasureType>(std::stoi(row.at("type")));
        m.subtype = row.at("subtype");
        if (row.find("geometry") != row.end())
            m.geometry = wktToGeometry(row.at("geometry"));
        m.length = std::stod(row.at("length"));
        m.area = std::stod(row.at("area"));
        m.status = row.at("status");
        m.condition = row.at("condition");
        if (row.find("vegetation_coverage") != row.end() && row.at("vegetation_coverage") != "NULL")
        {
            m.vegetation_coverage = std::stod(row.at("vegetation_coverage"));
        }
        m.period_id = std::stoi(row.at("period_id"));
        m.created_at = stringToTimestamp(row.at("created_at"));
        return m;
    }
}  // namespace soilwatercm::dao
