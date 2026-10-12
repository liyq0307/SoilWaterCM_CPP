#include "report_dao.h"
namespace soilwatercm::dao
{
    ReportDao::ReportDao(std::shared_ptr<repository::PostgresRepository> repo) : DaoBase<models::Report>(repo) {}
    std::optional<models::Report> ReportDao::findById(int id)
    {
        std::string sql = buildSelectQuery("reports",
                                           {"id", "type", "title", "docx_path", "pdf_path", "is_filed", "filing_number",
                                            "filing_date", "project_id", "period_ids", "created_at"},
                                           "id = " + std::to_string(id));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRowToReport(result[0]);
    }
    std::vector<models::Report> ReportDao::findByProjectId(int projectId)
    {
        std::string sql = buildSelectQuery("reports",
                                           {"id", "type", "title", "docx_path", "pdf_path", "is_filed", "filing_number",
                                            "filing_date", "project_id", "period_ids", "created_at"},
                                           "project_id = " + std::to_string(projectId) + " ORDER BY created_at DESC");
        auto result = repo_->executeQuery(sql);
        std::vector<models::Report> reports;
        for (const auto& row : result)
            reports.push_back(mapRowToReport(row));
        return reports;
    }
    std::vector<models::Report> ReportDao::list(int page, int pageSize, const std::string& orderBy, bool ascending)
    {
        std::string sql =
            buildSelectQuery("reports", {"id", "type", "title", "docx_path", "pdf_path", "is_filed", "filing_number",
                                         "filing_date", "project_id", "period_ids", "created_at"});
        sql += buildPagination(page, pageSize, orderBy, ascending);
        auto result = repo_->executeQuery(sql);
        std::vector<models::Report> reports;
        for (const auto& row : result)
            reports.push_back(mapRowToReport(row));
        return reports;
    }
    int ReportDao::insert(const models::Report& r)
    {
        std::string sql = buildInsertQuery(
            "reports", {"type", "title", "docx_path", "pdf_path", "is_filed", "project_id", "created_at"},
            {std::to_string(static_cast<int>(r.type)), "'" + escapeString(r.title) + "'",
             "'" + escapeString(r.docx_path) + "'", "'" + escapeString(r.pdf_path) + "'", r.is_filed ? "TRUE" : "FALSE",
             std::to_string(r.project_id), "'" + timestampToString(r.created_at) + "'"});
        auto result = repo_->executeQuery(sql);
        return result.empty() ? -1 : std::stoi(result[0]["id"]);
    }
    bool ReportDao::update(const models::Report& r)
    {
        std::string sql = buildUpdateQuery("reports", {"title", "docx_path", "pdf_path"},
                                           {"'" + escapeString(r.title) + "'", "'" + escapeString(r.docx_path) + "'",
                                            "'" + escapeString(r.pdf_path) + "'"},
                                           "id = " + std::to_string(r.id));
        return repo_->executeUpdate(sql);
    }
    bool ReportDao::remove(int id)
    {
        return repo_->executeUpdate(buildDeleteQuery("reports", "id = " + std::to_string(id)));
    }
    int ReportDao::count()
    {
        auto result = repo_->executeQuery(buildCountQuery("reports"));
        return result.empty() ? 0 : std::stoi(result[0]["count"]);
    }
    bool ReportDao::markAsFiled(int id, const std::string& filingNumber)
    {
        auto now = std::chrono::system_clock::now();
        std::string sql = "UPDATE reports SET is_filed = TRUE, filing_number = '" + escapeString(filingNumber) +
                          "', filing_date = '" + timestampToString(now) + "' WHERE id = " + std::to_string(id);
        return repo_->executeUpdate(sql);
    }
    std::map<std::string, std::string> ReportDao::getTaskProgress(int taskId)
    {
        std::map<std::string, std::string> progress;
        std::string sql =
            buildSelectQuery("tasks", {"id", "status", "progress", "COALESCE(error_message, '') as error_message"},
                             "id = " + std::to_string(taskId));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return progress;
        progress["task_id"] = result[0]["id"];
        progress["status"] = result[0]["status"];
        progress["progress"] = result[0]["progress"];
        if (result[0].count("error_message"))
        {
            progress["message"] = result[0]["error_message"];
        }
        return progress;
    }

    std::optional<std::string> ReportDao::getOpinion(int id)
    {
        // 对齐 Python reports.opinion_text; C++ 库可能缺列, 先惰性补列 (DDL 走 executeQuery 以正常提交)
        repo_->executeQuery("ALTER TABLE reports ADD COLUMN IF NOT EXISTS opinion_text TEXT");
        std::string sql = buildSelectQuery("reports", {"COALESCE(opinion_text, '') as opinion_text"},
                                           "id = " + std::to_string(id));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        auto it = result[0].find("opinion_text");
        if (it == result[0].end())
            return std::string("");
        return it->second;
    }

    bool ReportDao::updateOpinion(int id, const std::string& opinionText)
    {
        repo_->executeQuery("ALTER TABLE reports ADD COLUMN IF NOT EXISTS opinion_text TEXT");
        std::string sql = "UPDATE reports SET opinion_text = '" + escapeString(opinionText) + "' WHERE id = " +
                          std::to_string(id);
        return repo_->executeUpdate(sql);
    }

    std::map<std::string, double> ReportDao::previewStats(int projectId, int periodId)
    {
        std::map<std::string, double> stats;

        auto queryOne = [this](const std::string& sql) -> double
        {
            auto result = repo_->executeQuery(sql);
            if (result.empty() || result[0].empty())
                return 0.0;
            try
            {
                return std::stod(result[0].begin()->second);
            }
            catch (...)
            {
                return 0.0;
            }
        };

        std::string periodCond = periodId > 0 ? " AND period_id = " + std::to_string(periodId) : "";

        stats["disturbance_count"] = queryOne("SELECT COUNT(*) FROM disturbances d JOIN monitoring_periods mp "
                                              "ON d.period_id = mp.id WHERE mp.project_id = " +
                                              std::to_string(projectId) + periodCond);
        stats["disturbance_area_m2"] =
            queryOne("SELECT COALESCE(SUM(d.area), 0) FROM disturbances d JOIN monitoring_periods mp "
                     "ON d.period_id = mp.id WHERE mp.project_id = " +
                     std::to_string(projectId) + periodCond);
        stats["anomaly_total"] = queryOne("SELECT COUNT(*) FROM anomalies WHERE project_id = " +
                                          std::to_string(projectId) + periodCond);
        stats["anomaly_active"] = queryOne("SELECT COUNT(*) FROM anomalies WHERE project_id = " +
                                           std::to_string(projectId) + periodCond + " AND status <> " +
                                           std::to_string(static_cast<int>(models::DisposalStatus::Completed)));
        if (periodId > 0)
        {
            stats["mean_fvc"] =
                queryOne("SELECT COALESCE(AVG(mean_fvc), 0) FROM vegetation_coverages WHERE period_id = " +
                         std::to_string(periodId));
            stats["mean_erosion_modulus"] =
                queryOne("SELECT COALESCE(AVG(mean_modulus), 0) FROM erosion_results WHERE period_id = " +
                         std::to_string(periodId));
        }
        else
        {
            stats["mean_fvc"] = queryOne("SELECT COALESCE(AVG(vc.mean_fvc), 0) FROM vegetation_coverages vc "
                                         "JOIN monitoring_periods mp ON vc.period_id = mp.id WHERE mp.project_id = " +
                                         std::to_string(projectId));
            stats["mean_erosion_modulus"] =
                queryOne("SELECT COALESCE(AVG(er.mean_modulus), 0) FROM erosion_results er "
                         "JOIN monitoring_periods mp ON er.period_id = mp.id WHERE mp.project_id = " +
                         std::to_string(projectId));
        }

        return stats;
    }

    std::map<std::string, std::string> ReportDao::previewMeta(int projectId, int periodId)
    {
        std::map<std::string, std::string> meta;

        auto projectResult = repo_->executeQuery(
            buildSelectQuery("projects", {"id", "name", "code"}, "id = " + std::to_string(projectId)));
        if (!projectResult.empty())
        {
            meta["project_name"] = projectResult[0].count("name") ? projectResult[0]["name"] : "";
            meta["project_code"] = projectResult[0].count("code") ? projectResult[0]["code"] : "";
        }

        if (periodId > 0)
        {
            auto periodResult = repo_->executeQuery(
                buildSelectQuery("monitoring_periods", {"id", "period_code", "start_date", "end_date"},
                                 "id = " + std::to_string(periodId)));
            if (!periodResult.empty())
            {
                meta["period_code"] = periodResult[0].count("period_code") ? periodResult[0]["period_code"] : "";
                std::string start = periodResult[0].count("start_date") ? periodResult[0]["start_date"] : "";
                std::string end = periodResult[0].count("end_date") ? periodResult[0]["end_date"] : "";
                meta["period_range"] = start + " ~ " + end;
            }
        }

        return meta;
    }

    models::Report ReportDao::mapRowToReport(const std::map<std::string, std::string>& row)
    {
        models::Report r;
        r.id = std::stoi(row.at("id"));
        r.type = static_cast<models::ReportType>(std::stoi(row.at("type")));
        r.title = row.at("title");
        r.docx_path = row.at("docx_path");
        r.pdf_path = row.at("pdf_path");
        r.is_filed = (row.at("is_filed") == "t" || row.at("is_filed") == "TRUE");
        if (row.find("filing_number") != row.end() && row.at("filing_number") != "NULL")
        {
            r.filing_number = row.at("filing_number");
        }
        if (row.find("filing_date") != row.end() && row.at("filing_date") != "NULL")
        {
            r.filing_date = stringToTimestamp(row.at("filing_date"));
        }
        r.project_id = std::stoi(row.at("project_id"));
        r.created_at = stringToTimestamp(row.at("created_at"));
        return r;
    }
}  // namespace soilwatercm::dao
