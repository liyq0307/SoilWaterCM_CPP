#include "task_dao.h"
namespace soilwatercm::dao
{
    TaskDao::TaskDao(std::shared_ptr<repository::PostgresRepository> repo) : DaoBase<models::Task>(repo) {}
    std::optional<models::Task> TaskDao::findById(int id)
    {
        std::string sql =
            buildSelectQuery("tasks",
                             {"id", "type", "status", "progress", "params", "result_path", "error_message",
                              "submitted_at", "started_at", "completed_at", "retry_count", "project_id", "period_id"},
                             "id = " + std::to_string(id));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRowToTask(result[0]);
    }
    std::vector<models::Task> TaskDao::findByProjectId(int projectId)
    {
        std::string sql =
            buildSelectQuery("tasks",
                             {"id", "type", "status", "progress", "params", "result_path", "error_message",
                              "submitted_at", "started_at", "completed_at", "retry_count", "project_id", "period_id"},
                             "project_id = " + std::to_string(projectId) + " ORDER BY submitted_at DESC");
        auto result = repo_->executeQuery(sql);
        std::vector<models::Task> tasks;
        for (const auto& row : result)
            tasks.push_back(mapRowToTask(row));
        return tasks;
    }
    std::vector<models::Task> TaskDao::findByStatus(models::TaskStatus status)
    {
        std::string sql =
            buildSelectQuery("tasks",
                             {"id", "type", "status", "progress", "params", "result_path", "error_message",
                              "submitted_at", "started_at", "completed_at", "retry_count", "project_id", "period_id"},
                             "status = " + std::to_string(static_cast<int>(status)));
        auto result = repo_->executeQuery(sql);
        std::vector<models::Task> tasks;
        for (const auto& row : result)
            tasks.push_back(mapRowToTask(row));
        return tasks;
    }
    std::vector<models::Task> TaskDao::list(int page, int pageSize, const std::string& orderBy, bool ascending)
    {
        std::string sql = buildSelectQuery(
            "tasks", {"id", "type", "status", "progress", "params", "result_path", "error_message", "submitted_at",
                      "started_at", "completed_at", "retry_count", "project_id", "period_id"});
        sql += buildPagination(page, pageSize, orderBy, ascending);
        auto result = repo_->executeQuery(sql);
        std::vector<models::Task> tasks;
        for (const auto& row : result)
            tasks.push_back(mapRowToTask(row));
        return tasks;
    }
    int TaskDao::insert(const models::Task& t)
    {
        std::string sql =
            "INSERT INTO tasks (type, status, progress, params, result_path, error_message, submitted_at, started_at, "
            "completed_at, retry_count, project_id, period_id) VALUES (" +
            std::to_string(static_cast<int>(t.type)) + ", " + std::to_string(static_cast<int>(t.status)) + ", " +
            std::to_string(t.progress) + ", '" + escapeString(t.params) + "', " +
            (t.result_path ? "'" + escapeString(*t.result_path) + "'" : "NULL") + ", " +
            (t.error_message ? "'" + escapeString(*t.error_message) + "'" : "NULL") + ", '" +
            timestampToString(t.submitted_at) + "', " +
            (t.started_at ? "'" + timestampToString(*t.started_at) + "'" : "NULL") + ", " +
            (t.completed_at ? "'" + timestampToString(*t.completed_at) + "'" : "NULL") + ", " +
            std::to_string(t.retry_count) + ", " + std::to_string(t.project_id) + ", " + std::to_string(t.period_id) +
            ") RETURNING id";
        auto result = repo_->executeQuery(sql);
        return result.empty() ? -1 : std::stoi(result[0]["id"]);
    }
    bool TaskDao::update(const models::Task& t)
    {
        std::string sql = buildUpdateQuery("tasks", {"status", "progress"},
                                           {std::to_string(static_cast<int>(t.status)), std::to_string(t.progress)},
                                           "id = " + std::to_string(t.id));
        return repo_->executeUpdate(sql);
    }
    bool TaskDao::remove(int id)
    {
        return repo_->executeUpdate(buildDeleteQuery("tasks", "id = " + std::to_string(id)));
    }
    std::vector<models::Task> TaskDao::listRecent(int limit)
    {
        std::string sql = buildSelectQuery(
            "tasks", {"id", "type", "status", "progress", "params", "result_path", "error_message", "submitted_at",
                      "started_at", "completed_at", "retry_count", "project_id", "period_id"});
        sql += " ORDER BY submitted_at DESC LIMIT " + std::to_string(limit);
        auto result = repo_->executeQuery(sql);
        std::vector<models::Task> tasks;
        for (const auto& row : result)
            tasks.push_back(mapRowToTask(row));
        return tasks;
    }

    int TaskDao::count()
    {
        auto result = repo_->executeQuery(buildCountQuery("tasks"));
        return result.empty() ? 0 : std::stoi(result[0]["count"]);
    }
    bool TaskDao::updateStatus(int id, models::TaskStatus status, double progress)
    {
        std::string sql = "UPDATE tasks SET status = " + std::to_string(static_cast<int>(status)) +
                          ", progress = " + std::to_string(progress);
        if (status == models::TaskStatus::Running)
        {
            sql += ", started_at = '" + timestampToString(std::chrono::system_clock::now()) + "'";
        }
        else if (status == models::TaskStatus::Completed || status == models::TaskStatus::Failed)
        {
            sql += ", completed_at = '" + timestampToString(std::chrono::system_clock::now()) + "'";
        }
        sql += " WHERE id = " + std::to_string(id);
        return repo_->executeUpdate(sql);
    }
    bool TaskDao::updateStatusAndMessage(int id, models::TaskStatus status, double progress,
                                         const std::string& errorMessage)
    {
        std::string sql = "UPDATE tasks SET status = " + std::to_string(static_cast<int>(status)) +
                          ", progress = " + std::to_string(progress) + ", error_message = ";
        if (errorMessage.empty())
        {
            sql += "NULL";
        }
        else
        {
            sql += "'" + escapeString(errorMessage) + "'";
        }
        if (status == models::TaskStatus::Completed || status == models::TaskStatus::Failed)
        {
            sql += ", completed_at = '" + timestampToString(std::chrono::system_clock::now()) + "'";
        }
        else if (status == models::TaskStatus::Pending)
        {
            // 对齐 Python resume: 恢复排队时清空运行时间戳
            sql += ", started_at = NULL, completed_at = NULL";
        }
        sql += " WHERE id = " + std::to_string(id);
        return repo_->executeUpdate(sql);
    }
    bool TaskDao::updateResult(int id, const std::string& resultPath)
    {
        std::string sql =
            "UPDATE tasks SET result_path = '" + escapeString(resultPath) + "' WHERE id = " + std::to_string(id);
        return repo_->executeUpdate(sql);
    }
    bool TaskDao::incrementRetryCount(int id)
    {
        std::string sql = "UPDATE tasks SET retry_count = retry_count + 1 WHERE id = " + std::to_string(id);
        return repo_->executeUpdate(sql);
    }
    models::Task TaskDao::mapRowToTask(const std::map<std::string, std::string>& row)
    {
        models::Task t;
        t.id = std::stoi(row.at("id"));
        t.type = static_cast<models::TaskType>(std::stoi(row.at("type")));
        t.status = static_cast<models::TaskStatus>(std::stoi(row.at("status")));
        t.progress = std::stod(row.at("progress"));
        t.params = row.at("params");
        if (row.find("result_path") != row.end() && row.at("result_path") != "NULL")
        {
            t.result_path = row.at("result_path");
        }
        if (row.find("error_message") != row.end() && row.at("error_message") != "NULL")
        {
            t.error_message = row.at("error_message");
        }
        t.submitted_at = stringToTimestamp(row.at("submitted_at"));
        if (row.find("started_at") != row.end() && row.at("started_at") != "NULL")
        {
            t.started_at = stringToTimestamp(row.at("started_at"));
        }
        if (row.find("completed_at") != row.end() && row.at("completed_at") != "NULL")
        {
            t.completed_at = stringToTimestamp(row.at("completed_at"));
        }
        t.retry_count = std::stoi(row.at("retry_count"));
        t.project_id = std::stoi(row.at("project_id"));
        t.period_id = std::stoi(row.at("period_id"));
        return t;
    }
}  // namespace soilwatercm::dao
