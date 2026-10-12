#include "ground_truth_dao.h"

#include "utils/logger.h"

namespace soilwatercm::dao
{

    namespace
    {
        const char* kColumns[] = {
            "id",          "project_id", "period_id",      "sample_type",   "ST_AsText(location) as location",
            "survey_date", "surveyor",   "measured_value", "measured_unit", "target_table",
            "target_id",   "rs_value",   "error",          "accuracy_pass", "photo_paths",
            "note",        "created_at"};

        std::vector<std::string> columns()
        {
            return std::vector<std::string>(kColumns, kColumns + sizeof(kColumns) / sizeof(kColumns[0]));
        }

        std::string optIntToSql(const std::optional<int>& v)
        {
            return v.has_value() ? std::to_string(*v) : "NULL";
        }

        std::string optDoubleToSql(const std::optional<double>& v)
        {
            return v.has_value() ? std::to_string(*v) : "NULL";
        }

        std::string optBoolToIntSql(const std::optional<bool>& v)
        {
            if (!v.has_value())
                return "NULL";
            return *v ? "1" : "0";
        }

        std::string escapeForSql(const std::string& str)
        {
            std::string result;
            result.reserve(str.size());
            for (char c : str)
            {
                if (c == '\'')
                    result += "''";
                else if (c == '\\')
                    result += "\\\\";
                else
                    result += c;
            }
            return result;
        }

        std::string strOrNull(const std::string& v)
        {
            return v.empty() ? "NULL" : "'" + escapeForSql(v) + "'";
        }

        std::optional<double> parseOptDouble(const std::map<std::string, std::string>& row, const std::string& key)
        {
            auto it = row.find(key);
            if (it == row.end() || it->second.empty() || it->second == "NULL")
                return std::nullopt;
            try
            {
                return std::stod(it->second);
            }
            catch (const std::exception&)
            {
                return std::nullopt;
            }
        }

        std::optional<int> parseOptInt(const std::map<std::string, std::string>& row, const std::string& key)
        {
            auto it = row.find(key);
            if (it == row.end() || it->second.empty() || it->second == "NULL")
                return std::nullopt;
            try
            {
                return std::stoi(it->second);
            }
            catch (const std::exception&)
            {
                return std::nullopt;
            }
        }

        std::optional<bool> parseOptBool(const std::map<std::string, std::string>& row, const std::string& key)
        {
            auto it = row.find(key);
            if (it == row.end() || it->second.empty() || it->second == "NULL")
                return std::nullopt;
            return it->second == "1" || it->second == "t" || it->second == "TRUE";
        }

        std::string parseOptString(const std::map<std::string, std::string>& row, const std::string& key)
        {
            auto it = row.find(key);
            if (it == row.end() || it->second == "NULL")
                return "";
            return it->second;
        }
    }  // namespace

    GroundTruthDao::GroundTruthDao(std::shared_ptr<repository::PostgresRepository> repo)
        : DaoBase<models::GroundTruth>(repo)
    {
    }

    std::optional<models::GroundTruth> GroundTruthDao::findById(int id)
    {
        std::string sql = buildSelectQuery("ground_truths", columns(), "id = " + std::to_string(id));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRow(result[0]);
    }

    std::vector<models::GroundTruth> GroundTruthDao::list(int page, int pageSize, const std::string& orderBy,
                                                          bool ascending)
    {
        std::string sql = buildSelectQuery("ground_truths", columns());
        sql += buildPagination(page, pageSize, orderBy, ascending);
        auto result = repo_->executeQuery(sql);
        std::vector<models::GroundTruth> rows;
        for (const auto& r : result)
            rows.push_back(mapRow(r));
        return rows;
    }

    std::vector<models::GroundTruth> GroundTruthDao::list(std::optional<int> projectId, std::optional<int> periodId,
                                                          const std::string& sampleType, int page, int pageSize)
    {
        std::string where;
        if (projectId.has_value())
            where += (where.empty() ? "" : " AND ") + std::string("project_id = ") + std::to_string(*projectId);
        if (periodId.has_value())
            where += (where.empty() ? "" : " AND ") + std::string("period_id = ") + std::to_string(*periodId);
        if (!sampleType.empty())
            where += (where.empty() ? "" : " AND ") + std::string("sample_type = '") + escapeString(sampleType) + "'";

        std::string sql = buildSelectQuery("ground_truths", columns(), where);
        sql += buildPagination(page, pageSize, "survey_date", false);
        auto result = repo_->executeQuery(sql);
        std::vector<models::GroundTruth> rows;
        for (const auto& r : result)
            rows.push_back(mapRow(r));
        return rows;
    }

    std::vector<models::GroundTruth> GroundTruthDao::findByProject(int projectId)
    {
        std::string sql = buildSelectQuery("ground_truths", columns(), "project_id = " + std::to_string(projectId)) +
                          " ORDER BY survey_date DESC";
        auto result = repo_->executeQuery(sql);
        std::vector<models::GroundTruth> rows;
        for (const auto& r : result)
            rows.push_back(mapRow(r));
        return rows;
    }

    int GroundTruthDao::insert(const models::GroundTruth& gt)
    {
        std::string sql =
            "INSERT INTO ground_truths (project_id, period_id, sample_type, location, survey_date, "
            "surveyor, measured_value, measured_unit, target_table, target_id, rs_value, error, "
            "accuracy_pass, photo_paths, note, created_at) VALUES (" +
            std::to_string(gt.project_id) + ", " + optIntToSql(gt.period_id) + ", '" + escapeString(gt.sample_type) +
            "', ST_GeomFromText('" + escapeString(gt.location.wkt) + "', 4326), '" + timestampToString(gt.survey_date) +
            "', " + strOrNull(gt.surveyor) + ", " + optDoubleToSql(gt.measured_value) + ", " +
            strOrNull(gt.measured_unit) + ", " + strOrNull(gt.target_table) + ", " + optIntToSql(gt.target_id) + ", " +
            optDoubleToSql(gt.rs_value) + ", " + optDoubleToSql(gt.error) + ", " + optBoolToIntSql(gt.accuracy_pass) +
            ", " + strOrNull(gt.photo_paths) + ", " + strOrNull(gt.note) + ", '" + timestampToString(gt.created_at) +
            "') RETURNING id";
        auto result = repo_->executeQuery(sql);
        return result.empty() ? -1 : std::stoi(result[0]["id"]);
    }

    bool GroundTruthDao::update(const models::GroundTruth& gt)
    {
        std::string sql =
            "UPDATE ground_truths SET project_id = " + std::to_string(gt.project_id) +
            ", period_id = " + optIntToSql(gt.period_id) + ", sample_type = '" + escapeString(gt.sample_type) +
            "', location = " + "ST_GeomFromText('" + escapeString(gt.location.wkt) + "', 4326), survey_date = '" +
            timestampToString(gt.survey_date) + "', surveyor = " + strOrNull(gt.surveyor) +
            ", measured_value = " + optDoubleToSql(gt.measured_value) +
            ", measured_unit = " + strOrNull(gt.measured_unit) + ", target_table = " + strOrNull(gt.target_table) +
            ", target_id = " + optIntToSql(gt.target_id) + ", rs_value = " + optDoubleToSql(gt.rs_value) +
            ", error = " + optDoubleToSql(gt.error) + ", accuracy_pass = " + optBoolToIntSql(gt.accuracy_pass) +
            ", photo_paths = " + strOrNull(gt.photo_paths) + ", note = " + strOrNull(gt.note) +
            " WHERE id = " + std::to_string(gt.id);
        return repo_->executeUpdate(sql);
    }

    bool GroundTruthDao::remove(int id)
    {
        std::string sql = buildDeleteQuery("ground_truths", "id = " + std::to_string(id));
        return repo_->executeUpdate(sql);
    }

    int GroundTruthDao::count()
    {
        std::string sql = buildCountQuery("ground_truths");
        auto result = repo_->executeQuery(sql);
        return result.empty() ? 0 : std::stoi(result[0]["count"]);
    }

    std::optional<double> GroundTruthDao::fetchRsValue(const std::string& targetTable, int targetId)
    {
        std::string sql;
        if (targetTable == "disturbances")
            sql = buildSelectQuery("disturbances", {"area"}, "id = " + std::to_string(targetId)) + " LIMIT 1";
        else if (targetTable == "vegetation_coverages" || targetTable == "vegetation_coverage")
            sql =
                buildSelectQuery("vegetation_coverages", {"mean_fvc"}, "id = " + std::to_string(targetId)) + " LIMIT 1";
        else
            return std::nullopt;

        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;

        const auto& row = result[0];
        auto v = parseOptDouble(row, targetTable == "disturbances" ? "area" : "mean_fvc");
        return v;
    }

    models::GroundTruth GroundTruthDao::mapRow(const std::map<std::string, std::string>& row)
    {
        models::GroundTruth gt;
        gt.id = std::stoi(row.at("id"));
        gt.project_id = std::stoi(row.at("project_id"));
        gt.period_id = parseOptInt(row, "period_id");
        gt.sample_type = parseOptString(row, "sample_type");
        std::string locWkt = parseOptString(row, "location");
        if (!locWkt.empty())
            gt.location = wktToGeometry(locWkt);
        gt.survey_date = stringToTimestamp(row.at("survey_date"));
        gt.surveyor = parseOptString(row, "surveyor");
        gt.measured_value = parseOptDouble(row, "measured_value");
        gt.measured_unit = parseOptString(row, "measured_unit");
        gt.target_table = parseOptString(row, "target_table");
        gt.target_id = parseOptInt(row, "target_id");
        gt.rs_value = parseOptDouble(row, "rs_value");
        gt.error = parseOptDouble(row, "error");
        gt.accuracy_pass = parseOptBool(row, "accuracy_pass");
        gt.photo_paths = parseOptString(row, "photo_paths");
        gt.note = parseOptString(row, "note");
        gt.created_at = stringToTimestamp(row.at("created_at"));
        return gt;
    }

}  // namespace soilwatercm::dao
