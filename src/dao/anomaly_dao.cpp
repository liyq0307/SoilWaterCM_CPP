#include "anomaly_dao.h"
namespace soilwatercm::dao
{
    AnomalyDao::AnomalyDao(std::shared_ptr<repository::PostgresRepository> repo) : DaoBase<models::Anomaly>(repo) {}
    std::optional<models::Anomaly> AnomalyDao::findById(int id)
    {
        std::string sql =
            buildSelectQuery("anomalies",
                             {"id", "type", "ST_AsText(geometry) as geometry", "severity", "description", "status",
                              "assignee", "measures", "detected_at", "resolved_at", "project_id", "period_id"},
                             "id = " + std::to_string(id));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRowToAnomaly(result[0]);
    }
    std::vector<models::Anomaly> AnomalyDao::findByProjectId(int projectId)
    {
        std::string sql =
            buildSelectQuery("anomalies",
                             {"id", "type", "ST_AsText(geometry) as geometry", "severity", "description", "status",
                              "assignee", "measures", "detected_at", "resolved_at", "project_id", "period_id"},
                             "project_id = " + std::to_string(projectId) + " ORDER BY detected_at DESC");
        auto result = repo_->executeQuery(sql);
        std::vector<models::Anomaly> anomalies;
        for (const auto& row : result)
            anomalies.push_back(mapRowToAnomaly(row));
        return anomalies;
    }
    std::vector<models::Anomaly> AnomalyDao::list(int page, int pageSize, const std::string& orderBy, bool ascending)
    {
        std::string sql = buildSelectQuery(
            "anomalies", {"id", "type", "ST_AsText(geometry) as geometry", "severity", "description", "status",
                          "assignee", "measures", "detected_at", "resolved_at", "project_id", "period_id"});
        sql += buildPagination(page, pageSize, orderBy, ascending);
        auto result = repo_->executeQuery(sql);
        std::vector<models::Anomaly> anomalies;
        for (const auto& row : result)
            anomalies.push_back(mapRowToAnomaly(row));
        return anomalies;
    }
    int AnomalyDao::insert(const models::Anomaly& a)
    {
        std::string sql =
            "INSERT INTO anomalies (type, geometry, severity, description, status, assignee, measures, detected_at, "
            "resolved_at, project_id, period_id) VALUES (" +
            std::to_string(static_cast<int>(a.type)) + ", ST_GeomFromText('" + escapeString(a.geometry.wkt) +
            "', 4326), " + std::to_string(static_cast<int>(a.severity)) + ", '" + escapeString(a.description) + "', " +
            std::to_string(static_cast<int>(a.status)) + ", " +
            (a.assignee ? "'" + escapeString(*a.assignee) + "'" : "NULL") + ", " +
            (a.measures ? "'" + escapeString(*a.measures) + "'" : "NULL") + ", '" + timestampToString(a.detected_at) +
            "', " + (a.resolved_at ? "'" + timestampToString(*a.resolved_at) + "'" : "NULL") + ", " +
            std::to_string(a.project_id) + ", " + std::to_string(a.period_id) + ") RETURNING id";
        auto result = repo_->executeQuery(sql);
        return result.empty() ? -1 : std::stoi(result[0]["id"]);
    }
    bool AnomalyDao::update(const models::Anomaly& a)
    {
        std::string sql =
            buildUpdateQuery("anomalies", {"status", "description"},
                             {std::to_string(static_cast<int>(a.status)), "'" + escapeString(a.description) + "'"},
                             "id = " + std::to_string(a.id));
        return repo_->executeUpdate(sql);
    }
    bool AnomalyDao::remove(int id)
    {
        return repo_->executeUpdate(buildDeleteQuery("anomalies", "id = " + std::to_string(id)));
    }
    int AnomalyDao::count()
    {
        auto result = repo_->executeQuery(buildCountQuery("anomalies"));
        return result.empty() ? 0 : std::stoi(result[0]["count"]);
    }
    std::vector<models::Anomaly> AnomalyDao::findByStatus(models::DisposalStatus status)
    {
        std::string sql =
            buildSelectQuery("anomalies",
                             {"id", "type", "ST_AsText(geometry) as geometry", "severity", "description", "status",
                              "assignee", "measures", "detected_at", "resolved_at", "project_id", "period_id"},
                             "status = " + std::to_string(static_cast<int>(status)) + " ORDER BY detected_at DESC");
        auto result = repo_->executeQuery(sql);
        std::vector<models::Anomaly> anomalies;
        for (const auto& row : result)
            anomalies.push_back(mapRowToAnomaly(row));
        return anomalies;
    }

    bool AnomalyDao::updateAssignee(int id, int assigneeId)
    {
        std::string sql =
            "UPDATE anomalies SET assignee = '" + std::to_string(assigneeId) + "' WHERE id = " + std::to_string(id);
        return repo_->executeUpdate(sql);
    }

    bool AnomalyDao::updateDisposalStatus(int id, models::DisposalStatus status)
    {
        std::string sql = "UPDATE anomalies SET status = " + std::to_string(static_cast<int>(status)) +
                          " WHERE id = " + std::to_string(id);
        return repo_->executeUpdate(sql);
    }
    bool AnomalyDao::updateDetails(const models::Anomaly& a)
    {
        std::string sql = "UPDATE anomalies SET status = " + std::to_string(static_cast<int>(a.status)) +
                          ", severity = " + std::to_string(static_cast<int>(a.severity)) + ", description = '" +
                          escapeString(a.description) + "', measures = " +
                          (a.measures ? "'" + escapeString(*a.measures) + "'" : "NULL") + ", assignee = " +
                          (a.assignee ? "'" + escapeString(*a.assignee) + "'" : "NULL") + ", resolved_at = " +
                          (a.resolved_at ? "'" + timestampToString(*a.resolved_at) + "'" : "NULL") +
                          " WHERE id = " + std::to_string(a.id);
        return repo_->executeUpdate(sql);
    }
    models::Anomaly AnomalyDao::mapRowToAnomaly(const std::map<std::string, std::string>& row)
    {
        models::Anomaly a;
        a.id = std::stoi(row.at("id"));
        a.type = static_cast<models::AnomalyType>(std::stoi(row.at("type")));
        if (row.find("geometry") != row.end())
            a.geometry = wktToGeometry(row.at("geometry"));
        a.severity = static_cast<models::Severity>(std::stoi(row.at("severity")));
        a.description = row.at("description");
        a.status = static_cast<models::DisposalStatus>(std::stoi(row.at("status")));
        if (row.find("assignee") != row.end() && row.at("assignee") != "NULL")
            a.assignee = row.at("assignee");
        if (row.find("measures") != row.end() && row.at("measures") != "NULL")
            a.measures = row.at("measures");
        a.detected_at = stringToTimestamp(row.at("detected_at"));
        if (row.find("resolved_at") != row.end() && row.at("resolved_at") != "NULL")
        {
            a.resolved_at = stringToTimestamp(row.at("resolved_at"));
        }
        a.project_id = std::stoi(row.at("project_id"));
        a.period_id = std::stoi(row.at("period_id"));
        return a;
    }
}  // namespace soilwatercm::dao
