#include "trained_model_dao.h"

#include <optional>

namespace soilwatercm::dao
{
    static const std::vector<std::string> TRAINED_MODEL_FIELDS = {
        "id", "project_id", "period_id", "image_id", "source_task_id", "method", "model_name", "model_path",
        "status", "metrics", "file_size", "file_sha256", "created_at"};

    TrainedModelDao::TrainedModelDao(std::shared_ptr<repository::PostgresRepository> repo)
        : DaoBase<models::TrainedModel>(repo)
    {
    }

    std::optional<models::TrainedModel> TrainedModelDao::findById(int id)
    {
        std::string sql = buildSelectQuery("trained_models", TRAINED_MODEL_FIELDS, "id = " + std::to_string(id));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRow(result[0]);
    }

    std::vector<models::TrainedModel> TrainedModelDao::list(int page, int pageSize, const std::string& orderBy,
                                                             bool ascending)
    {
        std::string sql = buildSelectQuery("trained_models", TRAINED_MODEL_FIELDS);
        sql += buildPagination(page, pageSize, orderBy, ascending);
        auto result = repo_->executeQuery(sql);
        std::vector<models::TrainedModel> rows;
        for (const auto& row : result)
            rows.push_back(mapRow(row));
        return rows;
    }

    int TrainedModelDao::insert(const models::TrainedModel& tm)
    {
        std::string sql = buildInsertQuery(
            "trained_models",
            {"project_id", "period_id", "image_id", "source_task_id", "method", "model_name", "model_path", "status",
             "metrics", "file_size", "file_sha256", "created_at"},
            {std::to_string(tm.project_id),
             tm.period_id ? std::to_string(*tm.period_id) : "NULL",
             tm.image_id ? std::to_string(*tm.image_id) : "NULL",
             tm.source_task_id ? std::to_string(*tm.source_task_id) : "NULL",
             "'" + escapeString(tm.method) + "'",
             "'" + escapeString(tm.model_name) + "'",
             "'" + escapeString(tm.model_path) + "'",
             "'" + escapeString(tm.status) + "'",
             tm.metrics_json.empty() ? "NULL" : "'" + escapeString(tm.metrics_json) + "'",
             tm.file_size ? std::to_string(*tm.file_size) : "NULL",
             tm.file_sha256 ? "'" + escapeString(*tm.file_sha256) + "'" : "NULL",
             "'" + timestampToString(tm.created_at) + "'"});
        auto result = repo_->executeQuery(sql);
        return result.empty() ? -1 : std::stoi(result[0]["id"]);
    }

    bool TrainedModelDao::update(const models::TrainedModel& tm)
    {
        std::string sql = buildUpdateQuery(
            "trained_models",
            {"method", "model_name", "status", "metrics", "file_size", "file_sha256", "updated_at"},
            {"'" + escapeString(tm.method) + "'",
             "'" + escapeString(tm.model_name) + "'",
             "'" + escapeString(tm.status) + "'",
             tm.metrics_json.empty() ? "NULL" : "'" + escapeString(tm.metrics_json) + "'",
             tm.file_size ? std::to_string(*tm.file_size) : "NULL",
             tm.file_sha256 ? "'" + escapeString(*tm.file_sha256) + "'" : "NULL",
             "now()"},
            "id = " + std::to_string(tm.id));
        return repo_->executeUpdate(sql);
    }

    bool TrainedModelDao::remove(int id)
    {
        return repo_->executeUpdate(buildDeleteQuery("trained_models", "id = " + std::to_string(id)));
    }

    int TrainedModelDao::count()
    {
        auto result = repo_->executeQuery(buildCountQuery("trained_models"));
        return result.empty() ? 0 : std::stoi(result[0]["count"]);
    }

    std::vector<models::TrainedModel> TrainedModelDao::findByProject(int projectId, bool includeArchived)
    {
        std::string sql =
            buildSelectQuery("trained_models", TRAINED_MODEL_FIELDS, "project_id = " + std::to_string(projectId));
        if (!includeArchived)
            sql += " AND status = 'available'";
        sql += " ORDER BY created_at DESC LIMIT 500";
        auto result = repo_->executeQuery(sql);
        std::vector<models::TrainedModel> rows;
        for (const auto& row : result)
            rows.push_back(mapRow(row));
        return rows;
    }

    std::optional<models::TrainedModel> TrainedModelDao::latestByProject(int projectId)
    {
        std::string sql = buildSelectQuery("trained_models", TRAINED_MODEL_FIELDS,
                                           "project_id = " + std::to_string(projectId) + " AND status = 'available'");
        sql += " ORDER BY created_at DESC LIMIT 1";
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRow(result[0]);
    }

    bool TrainedModelDao::archive(const models::TrainedModel& tm)
    {
        std::string sql = "UPDATE trained_models SET status = 'archived', updated_at = now() WHERE id = " +
                          std::to_string(tm.id);
        return repo_->executeUpdate(sql);
    }

    models::TrainedModel TrainedModelDao::mapRow(const std::map<std::string, std::string>& row)
    {
        models::TrainedModel tm;
        tm.id = std::stoi(row.at("id"));
        tm.project_id = std::stoi(row.at("project_id"));
        auto optInt = [&](const std::string& key) -> std::optional<long long>
        {
            auto it = row.find(key);
            if (it != row.end() && !it->second.empty())
                return std::stoll(it->second);
            return std::nullopt;
        };
        auto periodIt = row.find("period_id");
        if (periodIt != row.end() && !periodIt->second.empty())
            tm.period_id = std::stoi(periodIt->second);
        auto imageIt = row.find("image_id");
        if (imageIt != row.end() && !imageIt->second.empty())
            tm.image_id = std::stoi(imageIt->second);
        auto taskIt = row.find("source_task_id");
        if (taskIt != row.end() && !taskIt->second.empty())
            tm.source_task_id = std::stoll(taskIt->second);
        tm.method = row.at("method");
        tm.model_name = row.at("model_name");
        tm.model_path = row.at("model_path");
        tm.status = row.at("status");
        auto metricsIt = row.find("metrics");
        if (metricsIt != row.end())
            tm.metrics_json = metricsIt->second;
        tm.file_size = optInt("file_size");
        auto shaIt = row.find("file_sha256");
        if (shaIt != row.end() && !shaIt->second.empty())
            tm.file_sha256 = shaIt->second;
        tm.created_at = stringToTimestamp(row.at("created_at"));
        return tm;
    }
}  // namespace soilwatercm::dao
