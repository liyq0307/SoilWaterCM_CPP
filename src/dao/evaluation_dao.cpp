#include "evaluation_dao.h"
namespace soilwatercm::dao
{
    EvaluationDao::EvaluationDao(std::shared_ptr<repository::PostgresRepository> repo)
        : DaoBase<models::Evaluation>(repo)
    {
    }
    std::optional<models::Evaluation> EvaluationDao::findById(int id)
    {
        std::string sql = buildSelectQuery("evaluations",
                                           {"id", "compliance_score", "compliance_level", "risk_score", "risk_level",
                                            "indicator_weights", "indicator_values", "period_id", "created_at"},
                                           "id = " + std::to_string(id));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRowToEvaluation(result[0]);
    }
    std::optional<models::Evaluation> EvaluationDao::findByPeriodId(int periodId)
    {
        std::string sql = buildSelectQuery("evaluations",
                                           {"id", "compliance_score", "compliance_level", "risk_score", "risk_level",
                                            "indicator_weights", "indicator_values", "period_id", "created_at"},
                                           "period_id = " + std::to_string(periodId));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRowToEvaluation(result[0]);
    }
    std::vector<models::Evaluation> EvaluationDao::list(int page, int pageSize, const std::string& orderBy,
                                                        bool ascending)
    {
        std::string sql =
            buildSelectQuery("evaluations", {"id", "compliance_score", "compliance_level", "risk_score", "risk_level",
                                             "indicator_weights", "indicator_values", "period_id", "created_at"});
        sql += buildPagination(page, pageSize, orderBy, ascending);
        auto result = repo_->executeQuery(sql);
        std::vector<models::Evaluation> evals;
        for (const auto& row : result)
            evals.push_back(mapRowToEvaluation(row));
        return evals;
    }
    int EvaluationDao::insert(const models::Evaluation& e)
    {
        std::string sql = buildInsertQuery(
            "evaluations",
            {"compliance_score", "compliance_level", "risk_score", "risk_level", "period_id", "created_at"},
            {std::to_string(e.compliance_score), std::to_string(static_cast<int>(e.compliance_level)),
             std::to_string(e.risk_score), std::to_string(static_cast<int>(e.risk_level)), std::to_string(e.period_id),
             "'" + timestampToString(e.created_at) + "'"});
        auto result = repo_->executeQuery(sql);
        return result.empty() ? -1 : std::stoi(result[0]["id"]);
    }
    bool EvaluationDao::update(const models::Evaluation& e)
    {
        std::string sql =
            buildUpdateQuery("evaluations", {"compliance_score", "compliance_level", "risk_score", "risk_level"},
                             {std::to_string(e.compliance_score), std::to_string(static_cast<int>(e.compliance_level)),
                              std::to_string(e.risk_score), std::to_string(static_cast<int>(e.risk_level))},
                             "id = " + std::to_string(e.id));
        return repo_->executeUpdate(sql);
    }
    bool EvaluationDao::upsert(const models::Evaluation& e)
    {
        auto existing = findByPeriodId(e.period_id);
        if (existing)
        {
            models::Evaluation updated = e;
            updated.id = existing->id;
            return update(updated);
        }
        return insert(e) > 0;
    }
    bool EvaluationDao::remove(int id)
    {
        return repo_->executeUpdate(buildDeleteQuery("evaluations", "id = " + std::to_string(id)));
    }
    int EvaluationDao::count()
    {
        auto result = repo_->executeQuery(buildCountQuery("evaluations"));
        return result.empty() ? 0 : std::stoi(result[0]["count"]);
    }
    models::Evaluation EvaluationDao::mapRowToEvaluation(const std::map<std::string, std::string>& row)
    {
        models::Evaluation e;
        e.id = std::stoi(row.at("id"));
        e.compliance_score = std::stod(row.at("compliance_score"));
        e.compliance_level = static_cast<models::ComplianceLevel>(std::stoi(row.at("compliance_level")));
        e.risk_score = std::stod(row.at("risk_score"));
        e.risk_level = static_cast<models::RiskLevel>(std::stoi(row.at("risk_level")));
        e.period_id = std::stoi(row.at("period_id"));
        e.created_at = stringToTimestamp(row.at("created_at"));
        return e;
    }
}  // namespace soilwatercm::dao
