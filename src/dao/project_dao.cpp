#include "project_dao.h"

#include "utils/logger.h"

namespace soilwatercm::dao
{

    ProjectDao::ProjectDao(std::shared_ptr<repository::PostgresRepository> repo) : DaoBase<models::Project>(repo) {}

    std::optional<models::Project> ProjectDao::findById(int id)
    {
        std::string sql =
            buildSelectQuery("projects",
                             {"id", "code", "name", "COALESCE(ST_AsText(responsibility_area), '') as responsibility_area",
                              "center_lon", "center_lat", "status", "created_at", "updated_at"},
                             "id = " + std::to_string(id));

        auto result = repo_->executeQuery(sql);
        if (result.empty())
        {
            return std::nullopt;
        }

        return mapRowToProject(result[0]);
    }

    std::optional<models::Project> ProjectDao::findByCode(const std::string& code)
    {
        std::string sql =
            buildSelectQuery("projects",
                             {"id", "code", "name", "COALESCE(ST_AsText(responsibility_area), '') as responsibility_area",
                              "center_lon", "center_lat", "status", "created_at", "updated_at"},
                             "code = '" + escapeString(code) + "'");

        auto result = repo_->executeQuery(sql);
        if (result.empty())
        {
            return std::nullopt;
        }

        return mapRowToProject(result[0]);
    }

    std::vector<models::Project> ProjectDao::list(int page, int pageSize, const std::string& orderBy, bool ascending)
    {
        std::string sql =
            buildSelectQuery("projects", {"id", "code", "name", "COALESCE(ST_AsText(responsibility_area), '') as responsibility_area",
                                          "center_lon", "center_lat", "status", "created_at", "updated_at"});

        sql += buildPagination(page, pageSize, orderBy, ascending);

        auto result = repo_->executeQuery(sql);
        std::vector<models::Project> projects;

        for (const auto& row : result)
        {
            projects.push_back(mapRowToProject(row));
        }

        return projects;
    }

    std::vector<models::Project> ProjectDao::findByStatus(models::ProjectStatus status)
    {
        std::string sql =
            buildSelectQuery("projects",
                             {"id", "code", "name", "COALESCE(ST_AsText(responsibility_area), '') as responsibility_area",
                              "center_lon", "center_lat", "status", "created_at", "updated_at"},
                             "status = " + std::to_string(static_cast<int>(status)));

        auto result = repo_->executeQuery(sql);
        std::vector<models::Project> projects;

        for (const auto& row : result)
        {
            projects.push_back(mapRowToProject(row));
        }

        return projects;
    }

    int ProjectDao::insert(const models::Project& project)
    {
        std::string sql =
            "INSERT INTO projects (code, name, responsibility_area, center_lon, center_lat, status, created_at, "
            "updated_at) "
            "VALUES ('" +
            escapeString(project.code) +
            "', "
            "'" +
            escapeString(project.name) +
            "', "
            "ST_GeomFromText('" +
            escapeString(project.responsibility_area.wkt) + "', 4326), " + std::to_string(project.center_lon) + ", " +
            std::to_string(project.center_lat) + ", " + std::to_string(static_cast<int>(project.status)) +
            ", "
            "'" +
            timestampToString(project.created_at) +
            "', "
            "'" +
            timestampToString(project.updated_at) +
            "') "
            "RETURNING id";

        auto result = repo_->executeQuery(sql);
        if (result.empty() || result[0].find("id") == result[0].end())
        {
            return -1;
        }

        return std::stoi(result[0]["id"]);
    }

    bool ProjectDao::update(const models::Project& project)
    {
        std::string sql =
            "UPDATE projects SET "
            "name = '" +
            escapeString(project.name) +
            "', "
            "responsibility_area = ST_GeomFromText('" +
            escapeString(project.responsibility_area.wkt) +
            "', 4326), "
            "center_lon = " +
            std::to_string(project.center_lon) +
            ", "
            "center_lat = " +
            std::to_string(project.center_lat) +
            ", "
            "status = " +
            std::to_string(static_cast<int>(project.status)) +
            ", "
            "updated_at = '" +
            timestampToString(project.updated_at) +
            "' "
            "WHERE id = " +
            std::to_string(project.id);

        return repo_->executeUpdate(sql);
    }

    bool ProjectDao::remove(int id)
    {
        std::string deleteMonitoringPeriods =
            buildDeleteQuery("monitoring_periods", "project_id = " + std::to_string(id));
        repo_->executeUpdate(deleteMonitoringPeriods);

        std::string deleteImages = buildDeleteQuery("images", "project_id = " + std::to_string(id));
        repo_->executeUpdate(deleteImages);

        std::string deleteAnomalies = buildDeleteQuery("anomalies", "project_id = " + std::to_string(id));
        repo_->executeUpdate(deleteAnomalies);

        std::string deleteReports = buildDeleteQuery("reports", "project_id = " + std::to_string(id));
        repo_->executeUpdate(deleteReports);

        std::string sql = buildDeleteQuery("projects", "id = " + std::to_string(id));
        return repo_->executeUpdate(sql);
    }

    int ProjectDao::count()
    {
        std::string sql = buildCountQuery("projects");
        auto result = repo_->executeQuery(sql);
        if (result.empty())
        {
            return 0;
        }
        return std::stoi(result[0]["count"]);
    }

    bool ProjectDao::existsByCode(const std::string& code)
    {
        std::string sql = buildCountQuery("projects", "code = '" + escapeString(code) + "'");
        auto result = repo_->executeQuery(sql);
        if (result.empty())
        {
            return false;
        }
        return std::stoi(result[0]["count"]) > 0;
    }

    std::vector<models::Project> ProjectDao::search(const std::string& keyword)
    {
        std::string sql = buildSelectQuery(
            "projects",
            {"id", "code", "name", "COALESCE(ST_AsText(responsibility_area), '') as responsibility_area", "center_lon", "center_lat",
             "status", "created_at", "updated_at"},
            "code LIKE '%" + escapeString(keyword) + "%' OR name LIKE '%" + escapeString(keyword) + "%'");

        auto result = repo_->executeQuery(sql);
        std::vector<models::Project> projects;

        for (const auto& row : result)
        {
            projects.push_back(mapRowToProject(row));
        }

        return projects;
    }

    models::Project ProjectDao::mapRowToProject(const std::map<std::string, std::string>& row)
    {
        models::Project project;

        project.id = std::stoi(row.at("id"));
        project.code = row.at("code");
        project.name = row.at("name");

        if (row.find("responsibility_area") != row.end())
        {
            project.responsibility_area = wktToGeometry(row.at("responsibility_area"));
        }

        project.center_lon = std::stod(row.at("center_lon"));
        project.center_lat = std::stod(row.at("center_lat"));
        project.status = static_cast<models::ProjectStatus>(std::stoi(row.at("status")));
        project.created_at = stringToTimestamp(row.at("created_at"));
        project.updated_at = stringToTimestamp(row.at("updated_at"));

        return project;
    }

}  // namespace soilwatercm::dao
