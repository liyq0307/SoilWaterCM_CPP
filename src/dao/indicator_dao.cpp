#include "indicator_dao.h"

#include <algorithm>

#include "utils/logger.h"

namespace soilwatercm::dao
{

    namespace
    {
        const std::vector<std::string> kIndicatorFields = {
            "id",     "code",           "name",           "dimension",     "unit",      "description",
            "weight", "threshold_good", "threshold_warn", "threshold_bad", "direction", "is_active"};

        const std::vector<std::string> kIndicatorValueFields = {"id",           "project_id", "period_id",
                                                                "indicator_id", "raw_value",  "normalized_value",
                                                                "score",        "level",      "created_at"};

        std::string optDoubleToSql(const std::optional<double>& v)
        {
            return v.has_value() ? std::to_string(*v) : "NULL";
        }

        std::string escapeSql(const std::string& str)
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

        std::string optStringToSql(const std::optional<std::string>& v)
        {
            return v.has_value() && !v->empty() ? "'" + escapeSql(*v) + "'" : "NULL";
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

        std::optional<std::string> parseOptString(const std::map<std::string, std::string>& row, const std::string& key)
        {
            auto it = row.find(key);
            if (it == row.end() || it->second.empty() || it->second == "NULL")
                return std::nullopt;
            return it->second;
        }

        double parseDouble(const std::map<std::string, std::string>& row, const std::string& key, double fallback = 0.0)
        {
            auto it = row.find(key);
            if (it == row.end() || it->second.empty() || it->second == "NULL")
                return fallback;
            try
            {
                return std::stod(it->second);
            }
            catch (const std::exception&)
            {
                return fallback;
            }
        }

        int parseInt(const std::map<std::string, std::string>& row, const std::string& key, int fallback = 0)
        {
            auto it = row.find(key);
            if (it == row.end() || it->second.empty() || it->second == "NULL")
                return fallback;
            try
            {
                return std::stoi(it->second);
            }
            catch (const std::exception&)
            {
                return fallback;
            }
        }
    }  // namespace

    IndicatorDao::IndicatorDao(std::shared_ptr<repository::PostgresRepository> repo) : DaoBase<models::Indicator>(repo)
    {
    }

    std::optional<models::Indicator> IndicatorDao::findById(int id)
    {
        std::string sql = buildSelectQuery("indicators", kIndicatorFields, "id = " + std::to_string(id)) + " LIMIT 1";
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRowToIndicator(result[0]);
    }

    std::vector<models::Indicator> IndicatorDao::list(int page, int pageSize, const std::string& orderBy,
                                                      bool ascending)
    {
        std::string sql = buildSelectQuery("indicators", kIndicatorFields);
        sql += buildPagination(page, pageSize, orderBy, ascending);
        auto result = repo_->executeQuery(sql);
        std::vector<models::Indicator> rows;
        for (const auto& r : result)
            rows.push_back(mapRowToIndicator(r));
        return rows;
    }

    std::vector<models::Indicator> IndicatorDao::listActive(const std::string& dimension)
    {
        std::string where = "is_active = 1";
        if (!dimension.empty())
            where += " AND dimension = '" + escapeString(dimension) + "'";

        std::string sql = buildSelectQuery("indicators", kIndicatorFields, where);
        sql += " ORDER BY dimension, id";
        auto result = repo_->executeQuery(sql);
        std::vector<models::Indicator> rows;
        for (const auto& r : result)
            rows.push_back(mapRowToIndicator(r));
        return rows;
    }

    std::optional<models::Indicator> IndicatorDao::findByCode(const std::string& code)
    {
        std::string sql =
            buildSelectQuery("indicators", kIndicatorFields, "code = '" + escapeString(code) + "'") + " LIMIT 1";
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRowToIndicator(result[0]);
    }

    int IndicatorDao::insert(const models::Indicator& indicator)
    {
        std::string sql =
            "INSERT INTO indicators (code, name, dimension, unit, description, weight, threshold_good, "
            "threshold_warn, threshold_bad, direction, is_active) VALUES ('" +
            escapeString(indicator.code) + "', '" + escapeString(indicator.name) + "', '" +
            escapeString(indicator.dimension) + "', " + optStringToSql(indicator.unit) + ", " +
            optStringToSql(indicator.description) + ", " + std::to_string(indicator.weight) + ", " +
            optDoubleToSql(indicator.threshold_good) + ", " + optDoubleToSql(indicator.threshold_warn) + ", " +
            optDoubleToSql(indicator.threshold_bad) + ", '" + escapeString(indicator.direction) + "', " +
            std::to_string(indicator.is_active) + ") RETURNING id";
        auto result = repo_->executeQuery(sql);
        return result.empty() ? -1 : std::stoi(result[0]["id"]);
    }

    bool IndicatorDao::update(const models::Indicator& indicator)
    {
        std::string sql =
            buildUpdateQuery("indicators",
                             {"code", "name", "dimension", "unit", "description", "weight", "threshold_good",
                              "threshold_warn", "threshold_bad", "direction", "is_active"},
                             {"'" + escapeString(indicator.code) + "'", "'" + escapeString(indicator.name) + "'",
                              "'" + escapeString(indicator.dimension) + "'", optStringToSql(indicator.unit),
                              optStringToSql(indicator.description), std::to_string(indicator.weight),
                              optDoubleToSql(indicator.threshold_good), optDoubleToSql(indicator.threshold_warn),
                              optDoubleToSql(indicator.threshold_bad), "'" + escapeString(indicator.direction) + "'",
                              std::to_string(indicator.is_active)},
                             "id = " + std::to_string(indicator.id));
        return repo_->executeUpdate(sql);
    }

    bool IndicatorDao::remove(int id)
    {
        std::string sql = buildDeleteQuery("indicators", "id = " + std::to_string(id));
        return repo_->executeUpdate(sql);
    }

    int IndicatorDao::count()
    {
        std::string sql = buildCountQuery("indicators");
        auto result = repo_->executeQuery(sql);
        return result.empty() ? 0 : std::stoi(result[0]["count"]);
    }

    bool IndicatorDao::updateWeight(int id, double weight)
    {
        std::string sql =
            "UPDATE indicators SET weight = " + std::to_string(weight) + " WHERE id = " + std::to_string(id);
        return repo_->executeUpdate(sql);
    }

    bool IndicatorDao::updateThresholds(int id, std::optional<double> good, std::optional<double> warn,
                                        std::optional<double> bad)
    {
        std::string sql = "UPDATE indicators SET threshold_good = " + optDoubleToSql(good) +
                          ", threshold_warn = " + optDoubleToSql(warn) + ", threshold_bad = " + optDoubleToSql(bad) +
                          " WHERE id = " + std::to_string(id);
        return repo_->executeUpdate(sql);
    }

    std::vector<models::IndicatorValue> IndicatorDao::listValues(int projectId, std::optional<int> periodId,
                                                                 std::optional<int> indicatorId)
    {
        std::string where = "project_id = " + std::to_string(projectId);
        if (periodId.has_value())
            where += " AND period_id = " + std::to_string(*periodId);
        if (indicatorId.has_value())
            where += " AND indicator_id = " + std::to_string(*indicatorId);

        std::string sql = buildSelectQuery("indicator_values", kIndicatorValueFields, where);
        sql += " ORDER BY id";
        auto result = repo_->executeQuery(sql);
        std::vector<models::IndicatorValue> rows;
        for (const auto& r : result)
            rows.push_back(mapRowToValue(r));
        return rows;
    }

    int IndicatorDao::insertValue(const models::IndicatorValue& value)
    {
        std::string sql =
            "INSERT INTO indicator_values (project_id, period_id, indicator_id, raw_value, normalized_value, "
            "score, level, created_at) VALUES (" +
            std::to_string(value.project_id) + ", " + std::to_string(value.period_id) + ", " +
            std::to_string(value.indicator_id) + ", " + std::to_string(value.raw_value) + ", " +
            optDoubleToSql(value.normalized_value) + ", " + optDoubleToSql(value.score) + ", " +
            (value.level.empty() ? "NULL" : "'" + escapeString(value.level) + "'") + ", '" +
            timestampToString(value.created_at) + "') RETURNING id";
        auto result = repo_->executeQuery(sql);
        return result.empty() ? -1 : std::stoi(result[0]["id"]);
    }

    int IndicatorDao::countValues(int projectId)
    {
        std::string sql = buildCountQuery("indicator_values", "project_id = " + std::to_string(projectId));
        auto result = repo_->executeQuery(sql);
        return result.empty() ? 0 : std::stoi(result[0]["count"]);
    }

    models::Indicator IndicatorDao::mapRowToIndicator(const std::map<std::string, std::string>& row)
    {
        models::Indicator ind;
        ind.id = parseInt(row, "id");
        ind.code = row.count("code") ? row.at("code") : "";
        ind.name = row.count("name") ? row.at("name") : "";
        ind.dimension = row.count("dimension") ? row.at("dimension") : "";
        ind.unit = parseOptString(row, "unit");
        ind.description = parseOptString(row, "description");
        ind.weight = parseDouble(row, "weight");
        ind.threshold_good = parseOptDouble(row, "threshold_good");
        ind.threshold_warn = parseOptDouble(row, "threshold_warn");
        ind.threshold_bad = parseOptDouble(row, "threshold_bad");
        ind.direction = row.count("direction") ? row.at("direction") : "lower_better";
        ind.is_active = parseInt(row, "is_active", 1);
        return ind;
    }

    models::IndicatorValue IndicatorDao::mapRowToValue(const std::map<std::string, std::string>& row)
    {
        models::IndicatorValue v;
        v.id = parseInt(row, "id");
        v.project_id = parseInt(row, "project_id");
        v.period_id = parseInt(row, "period_id");
        v.indicator_id = parseInt(row, "indicator_id");
        v.raw_value = parseDouble(row, "raw_value");
        v.normalized_value = parseOptDouble(row, "normalized_value");
        v.score = parseOptDouble(row, "score");
        v.level = parseOptString(row, "level").value_or("");
        auto it = row.find("created_at");
        if (it != row.end() && !it->second.empty() && it->second != "NULL")
            v.created_at = stringToTimestamp(it->second);
        else
            v.created_at = std::chrono::system_clock::now();
        return v;
    }

}  // namespace soilwatercm::dao
