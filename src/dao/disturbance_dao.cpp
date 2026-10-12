#include "disturbance_dao.h"

namespace soilwatercm::dao
{

    DisturbanceDao::DisturbanceDao(std::shared_ptr<repository::PostgresRepository> repo)
        : DaoBase<models::Disturbance>(repo)
    {
    }

    std::optional<models::Disturbance> DisturbanceDao::findById(int id)
    {
        std::string sql = buildSelectQuery("disturbances",
                                           {"id", "disturbance_code", "type", "ST_AsText(geometry) as geometry", "area",
                                            "perimeter", "confidence", "period_id", "created_at"},
                                           "id = " + std::to_string(id));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRowToDisturbance(result[0]);
    }

    std::vector<models::Disturbance> DisturbanceDao::findByPeriodId(int periodId)
    {
        std::string sql = buildSelectQuery("disturbances",
                                           {"id", "disturbance_code", "type", "ST_AsText(geometry) as geometry", "area",
                                            "perimeter", "confidence", "period_id", "created_at"},
                                           "period_id = " + std::to_string(periodId));
        auto result = repo_->executeQuery(sql);
        std::vector<models::Disturbance> disturbances;
        for (const auto& row : result)
            disturbances.push_back(mapRowToDisturbance(row));
        return disturbances;
    }

    std::vector<models::Disturbance> DisturbanceDao::findByProject(int projectId, std::optional<int> periodId)
    {
        std::string where = "mp.project_id = " + std::to_string(projectId);
        if (periodId.has_value())
            where += " AND d.period_id = " + std::to_string(*periodId);

        std::string sql =
            buildSelectQuery("disturbances d JOIN monitoring_periods mp ON d.period_id = mp.id",
                             {"d.id", "d.disturbance_code", "d.type", "ST_AsText(d.geometry) as geometry", "d.area",
                              "d.perimeter", "d.confidence", "d.period_id", "d.created_at"},
                             where);
        sql += " ORDER BY d.area DESC LIMIT 2000";
        auto result = repo_->executeQuery(sql);
        std::vector<models::Disturbance> disturbances;
        for (const auto& row : result)
            disturbances.push_back(mapRowToDisturbance(row));
        return disturbances;
    }

    std::vector<models::Disturbance> DisturbanceDao::list(int page, int pageSize, const std::string& orderBy,
                                                          bool ascending)
    {
        std::string sql =
            buildSelectQuery("disturbances", {"id", "disturbance_code", "type", "ST_AsText(geometry) as geometry",
                                              "area", "perimeter", "confidence", "period_id", "created_at"});
        sql += buildPagination(page, pageSize, orderBy, ascending);
        auto result = repo_->executeQuery(sql);
        std::vector<models::Disturbance> disturbances;
        for (const auto& row : result)
            disturbances.push_back(mapRowToDisturbance(row));
        return disturbances;
    }

    int DisturbanceDao::insert(const models::Disturbance& d)
    {
        std::string sql =
            "INSERT INTO disturbances (disturbance_code, type, geometry, area, perimeter, confidence, period_id, "
            "created_at) VALUES ('" +
            escapeString(d.disturbance_code) + "', " + std::to_string(static_cast<int>(d.type)) + ", " +
            "ST_GeomFromText('" + escapeString(d.geometry.wkt) + "', 4326), " + std::to_string(d.area) + ", " +
            std::to_string(d.perimeter) + ", " + std::to_string(d.confidence) + ", " + std::to_string(d.period_id) +
            ", '" + timestampToString(d.created_at) + "') RETURNING id";
        auto result = repo_->executeQuery(sql);
        return result.empty() ? -1 : std::stoi(result[0]["id"]);
    }

    std::vector<int> DisturbanceDao::batchInsert(const std::vector<models::Disturbance>& disturbances)
    {
        std::vector<int> ids;
        for (const auto& d : disturbances)
        {
            int id = insert(d);
            if (id > 0)
                ids.push_back(id);
        }
        return ids;
    }

    bool DisturbanceDao::update(const models::Disturbance& d)
    {
        std::string sql = buildUpdateQuery("disturbances", {"type", "area", "perimeter", "confidence"},
                                           {std::to_string(static_cast<int>(d.type)), std::to_string(d.area),
                                            std::to_string(d.perimeter), std::to_string(d.confidence)},
                                           "id = " + std::to_string(d.id));
        return repo_->executeUpdate(sql);
    }

    bool DisturbanceDao::remove(int id)
    {
        std::string sql = buildDeleteQuery("disturbances", "id = " + std::to_string(id));
        return repo_->executeUpdate(sql);
    }

    int DisturbanceDao::count()
    {
        std::string sql = buildCountQuery("disturbances");
        auto result = repo_->executeQuery(sql);
        return result.empty() ? 0 : std::stoi(result[0]["count"]);
    }

    models::Disturbance DisturbanceDao::mapRowToDisturbance(const std::map<std::string, std::string>& row)
    {
        models::Disturbance d;
        d.id = std::stoi(row.at("id"));
        d.disturbance_code = row.at("disturbance_code");
        d.type = static_cast<models::DisturbanceType>(std::stoi(row.at("type")));
        if (row.find("geometry") != row.end())
            d.geometry = wktToGeometry(row.at("geometry"));
        d.area = std::stod(row.at("area"));
        d.perimeter = std::stod(row.at("perimeter"));
        d.confidence = std::stod(row.at("confidence"));
        d.period_id = std::stoi(row.at("period_id"));
        d.created_at = stringToTimestamp(row.at("created_at"));
        return d;
    }

}  // namespace soilwatercm::dao
