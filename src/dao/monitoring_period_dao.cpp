#include "monitoring_period_dao.h"

namespace soilwatercm::dao
{

    MonitoringPeriodDao::MonitoringPeriodDao(std::shared_ptr<repository::PostgresRepository> repo)
        : DaoBase<models::MonitoringPeriod>(repo)
    {
    }

    std::optional<models::MonitoringPeriod> MonitoringPeriodDao::findById(int id)
    {
        std::string sql = buildSelectQuery(
            "monitoring_periods", {"id", "period_code", "type", "start_date", "end_date", "status", "project_id"},
            "id = " + std::to_string(id));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRowToPeriod(result[0]);
    }

    std::vector<models::MonitoringPeriod> MonitoringPeriodDao::findByProjectId(int projectId)
    {
        std::string sql = buildSelectQuery(
            "monitoring_periods", {"id", "period_code", "type", "start_date", "end_date", "status", "project_id"},
            "project_id = " + std::to_string(projectId) + " ORDER BY start_date DESC");
        auto result = repo_->executeQuery(sql);
        std::vector<models::MonitoringPeriod> periods;
        for (const auto& row : result)
            periods.push_back(mapRowToPeriod(row));
        return periods;
    }

    std::vector<models::MonitoringPeriod> MonitoringPeriodDao::list(int page, int pageSize, const std::string& orderBy,
                                                                    bool ascending)
    {
        std::string sql = buildSelectQuery(
            "monitoring_periods", {"id", "period_code", "type", "start_date", "end_date", "status", "project_id"});
        sql += buildPagination(page, pageSize, orderBy, ascending);
        auto result = repo_->executeQuery(sql);
        std::vector<models::MonitoringPeriod> periods;
        for (const auto& row : result)
            periods.push_back(mapRowToPeriod(row));
        return periods;
    }

    int MonitoringPeriodDao::insert(const models::MonitoringPeriod& period)
    {
        std::string sql = buildInsertQuery(
            "monitoring_periods", {"period_code", "type", "start_date", "end_date", "status", "project_id"},
            {"'" + escapeString(period.period_code) + "'", std::to_string(static_cast<int>(period.type)),
             "'" + timestampToString(period.start_date) + "'", "'" + timestampToString(period.end_date) + "'",
             std::to_string(static_cast<int>(period.status)), std::to_string(period.project_id)});
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return -1;
        return std::stoi(result[0]["id"]);
    }

    bool MonitoringPeriodDao::update(const models::MonitoringPeriod& period)
    {
        std::string sql = buildUpdateQuery(
            "monitoring_periods", {"period_code", "type", "start_date", "end_date", "status"},
            {"'" + escapeString(period.period_code) + "'", std::to_string(static_cast<int>(period.type)),
             "'" + timestampToString(period.start_date) + "'", "'" + timestampToString(period.end_date) + "'",
             std::to_string(static_cast<int>(period.status))},
            "id = " + std::to_string(period.id));
        return repo_->executeUpdate(sql);
    }

    bool MonitoringPeriodDao::remove(int id)
    {
        std::string sql = buildDeleteQuery("monitoring_periods", "id = " + std::to_string(id));
        return repo_->executeUpdate(sql);
    }

    int MonitoringPeriodDao::count()
    {
        std::string sql = buildCountQuery("monitoring_periods");
        auto result = repo_->executeQuery(sql);
        return result.empty() ? 0 : std::stoi(result[0]["count"]);
    }

    models::MonitoringPeriod MonitoringPeriodDao::mapRowToPeriod(const std::map<std::string, std::string>& row)
    {
        models::MonitoringPeriod period;
        period.id = std::stoi(row.at("id"));
        period.period_code = row.at("period_code");
        period.type = static_cast<models::PeriodType>(std::stoi(row.at("type")));
        period.start_date = stringToTimestamp(row.at("start_date"));
        period.end_date = stringToTimestamp(row.at("end_date"));
        period.status = static_cast<models::ProjectStatus>(std::stoi(row.at("status")));
        period.project_id = std::stoi(row.at("project_id"));
        return period;
    }

}  // namespace soilwatercm::dao
