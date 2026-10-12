#include "evaluation_service.h"

#include <cmath>
#include <numeric>

#include "../common/exceptions.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{

    EvaluationService::EvaluationService(std::shared_ptr<dao::EvaluationDao> evaluationDao,
                                         std::shared_ptr<dao::TaskDao> taskDao)
        : evaluationDao_(std::move(evaluationDao)), taskDao_(std::move(taskDao))
    {
    }

    std::future<EvaluationResult> EvaluationService::submitEvaluationTask(const EvaluationParams& params)
    {
        return std::async(std::launch::async,
                          [this, params]()
                          {
                              models::Task task;
                              task.type = models::TaskType::Evaluation;
                              task.status = models::TaskStatus::Pending;
                              task.submitted_at = std::chrono::system_clock::now();
                              task.period_id = params.period_id;

                              int taskId = taskDao_->insert(task);

                              return this->evaluate(params, taskId);
                          });
    }

    EvaluationResult EvaluationService::evaluate(const EvaluationParams& params, int taskId)
    {
        EvaluationResult result;
        result.task_id = taskId;

        try
        {
            updateTaskProgress(taskId, 0, "开始综合评价");

            std::map<std::string, double> weights;

            if (params.weights)
            {
                weights = *params.weights;
            }
            else
            {
                weights = calculateEntropyWeights({});
            }

            result.weights = weights;
            result.indicator_values = params.indicator_values;

            double weightedSum = 0.0;
            double weightSum = 0.0;

            for (const auto& [indicator, value] : params.indicator_values)
            {
                double w = weights.count(indicator) ? weights[indicator] : 1.0;
                weightedSum += w * value;
                weightSum += w;
            }

            double score = (weightSum > 0) ? weightedSum / weightSum : 0.0;

            result.compliance_score = score;
            result.compliance_level = evaluateCompliance(score);

            result.risk_score = 100.0 - score;
            result.risk_level = evaluateRisk(result.risk_score);

            updateTaskProgress(taskId, 80, "保存评价结果");

            models::Evaluation evaluation;
            evaluation.compliance_score = result.compliance_score;
            evaluation.compliance_level = result.compliance_level;
            evaluation.risk_score = result.risk_score;
            evaluation.risk_level = result.risk_level;
            evaluation.period_id = params.period_id;
            evaluation.created_at = std::chrono::system_clock::now();

            int evalId = evaluationDao_->insert(evaluation);
            evaluation.id = evalId;

            result.success = true;
            result.message = "综合评价完成";

            updateTaskProgress(taskId, 100, "综合评价完成");

            utils::Logger::info("Evaluation completed: period={}, score={}", params.period_id, result.compliance_score);
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.message = std::string("综合评价失败: ") + e.what();
            updateTaskProgress(taskId, -1, result.message);
        }

        return result;
    }

    std::map<std::string, double> EvaluationService::calculateAhpWeights(
        const std::map<std::string, std::map<std::string, double>>& comparisonMatrix)
    {
        std::map<std::string, double> weights;

        if (comparisonMatrix.empty())
        {
            return weights;
        }

        std::vector<std::string> indicators;
        for (const auto& [key, _] : comparisonMatrix)
        {
            indicators.push_back(key);
        }

        int n = static_cast<int>(indicators.size());
        if (n == 0)
            return weights;

        // Build the judgment matrix as a flat vector (row-major)
        std::vector<double> mat(n * n, 0.0);
        for (int i = 0; i < n; ++i)
        {
            for (int j = 0; j < n; ++j)
            {
                const std::string& rowKey = indicators[i];
                const std::string& colKey = indicators[j];
                if (comparisonMatrix.count(rowKey) && comparisonMatrix.at(rowKey).count(colKey))
                {
                    mat[i * n + j] = comparisonMatrix.at(rowKey).at(colKey);
                }
                else
                {
                    mat[i * n + j] = (i == j) ? 1.0 : 0.0;
                }
            }
        }

        // Power iteration for dominant eigenvalue/eigenvector
        std::vector<double> w(n, 1.0 / n);
        for (int iter = 0; iter < 100; ++iter)
        {
            std::vector<double> w2(n, 0.0);
            double sum = 0.0;
            for (int i = 0; i < n; ++i)
            {
                for (int j = 0; j < n; ++j)
                {
                    w2[i] += mat[i * n + j] * w[j];
                }
                sum += w2[i];
            }
            if (sum > 0)
            {
                for (int i = 0; i < n; ++i)
                    w2[i] /= sum;
            }
            double diff = 0.0;
            for (int i = 0; i < n; ++i)
                diff += std::abs(w2[i] - w[i]);
            w = w2;
            if (diff < 1e-6)
                break;
        }

        for (int i = 0; i < n; ++i)
        {
            weights[indicators[i]] = w[i];
        }

        return weights;
    }

    bool EvaluationService::checkConsistency(
        const std::map<std::string, std::map<std::string, double>>& comparisonMatrix,
        const std::map<std::string, double>& weights)
    {
        double cr = calculateConsistencyRatio(comparisonMatrix, weights);
        return cr < 0.1;
    }

    double EvaluationService::calculateConsistencyRatio(
        const std::map<std::string, std::map<std::string, double>>& matrix,
        const std::map<std::string, double>& weights)
    {
        const std::vector<double> randomIndex = {0.0,  0.0,  0.58, 0.90, 1.12, 1.24, 1.32, 1.41,
                                                 1.45, 1.49, 1.51, 1.54, 1.56, 1.58, 1.59};

        int n = static_cast<int>(weights.size());
        if (n < 2)
            return 0.0;

        std::vector<std::string> indicators;
        for (const auto& [key, _] : weights)
        {
            indicators.push_back(key);
        }

        std::vector<double> w(n);
        for (int i = 0; i < n; ++i)
        {
            w[i] = weights.at(indicators[i]);
        }

        double lambdaMax = 0.0;
        for (int i = 0; i < n; ++i)
        {
            double aw = 0.0;
            for (int j = 0; j < n; ++j)
            {
                const std::string& rowKey = indicators[i];
                const std::string& colKey = indicators[j];
                if (matrix.count(rowKey) && matrix.at(rowKey).count(colKey))
                {
                    aw += matrix.at(rowKey).at(colKey) * w[j];
                }
            }
            if (w[i] > 0)
            {
                lambdaMax += aw / w[i];
            }
        }

        lambdaMax /= n;
        double ci = (lambdaMax - n) / (n - 1);
        double ri = (n < static_cast<int>(randomIndex.size())) ? randomIndex[n] : 1.59;

        return (ri > 0) ? ci / ri : 0.0;
    }

    std::map<std::string, double> EvaluationService::calculateEntropyWeights(
        const std::map<std::string, std::vector<double>>& indicatorData)
    {
        std::map<std::string, double> weights;

        if (indicatorData.empty())
        {
            weights["indicator1"] = 0.25;
            weights["indicator2"] = 0.25;
            weights["indicator3"] = 0.25;
            weights["indicator4"] = 0.25;
            return weights;
        }

        std::vector<std::string> indicators;
        for (const auto& [key, _] : indicatorData)
        {
            indicators.push_back(key);
        }
        int n = static_cast<int>(indicators.size());

        int m = 0;
        for (const auto& [_, values] : indicatorData)
        {
            m = std::max(m, static_cast<int>(values.size()));
        }

        if (m < 2)
        {
            double equalWeight = 1.0 / n;
            for (const auto& key : indicators)
            {
                weights[key] = equalWeight;
            }
            return weights;
        }

        // Build matrix and min-max normalize
        std::vector<std::vector<double>> data(n, std::vector<double>(m, 0.0));
        for (int i = 0; i < n; ++i)
        {
            const auto& values = indicatorData.at(indicators[i]);
            double minVal = *std::min_element(values.begin(), values.end());
            double maxVal = *std::max_element(values.begin(), values.end());
            double range = maxVal - minVal;
            if (range < 1e-10)
                range = 1.0;

            for (int j = 0; j < static_cast<int>(values.size()) && j < m; ++j)
            {
                data[i][j] = (values[j] - minVal) / range;
            }
        }

        // Calculate column sums for probability normalization
        std::vector<double> colSum(m, 0.0);
        for (int j = 0; j < m; ++j)
        {
            for (int i = 0; i < n; ++i)
            {
                colSum[j] += data[i][j];
            }
            if (colSum[j] < 1e-10)
                colSum[j] = 1.0;
        }

        // Entropy: e_j = -k * sum(p * ln(p)), k = 1/ln(m)
        double k = 1.0 / std::log(static_cast<double>(m));
        std::vector<double> entropies(n, 0.0);

        for (int i = 0; i < n; ++i)
        {
            double entropy = 0.0;
            for (int j = 0; j < m; ++j)
            {
                double p = data[i][j] / colSum[j];
                if (p > 1e-12)
                {
                    entropy -= p * std::log(p);
                }
            }
            entropies[i] = k * entropy;
        }

        // Diversity: d_i = 1 - e_i, weight: w_i = d_i / sum(d_i)
        double diversitySum = 0.0;
        std::vector<double> diversities(n, 0.0);
        for (int i = 0; i < n; ++i)
        {
            diversities[i] = 1.0 - entropies[i];
            diversitySum += diversities[i];
        }

        if (diversitySum < 1e-10)
        {
            double equalWeight = 1.0 / n;
            for (const auto& key : indicators)
            {
                weights[key] = equalWeight;
            }
        }
        else
        {
            for (int i = 0; i < n; ++i)
            {
                weights[indicators[i]] = diversities[i] / diversitySum;
            }
        }

        return weights;
    }

    std::map<std::string, double> EvaluationService::combineWeights(const std::map<std::string, double>& ahpWeights,
                                                                    const std::map<std::string, double>& entropyWeights,
                                                                    double alpha)
    {
        std::map<std::string, double> combined;

        for (const auto& [key, ahpW] : ahpWeights)
        {
            double entropyW = entropyWeights.count(key) ? entropyWeights.at(key) : 0.0;
            combined[key] = alpha * ahpW + (1.0 - alpha) * entropyW;
        }

        return combined;
    }

    models::ComplianceLevel EvaluationService::evaluateCompliance(double score)
    {
        if (score >= 90)
            return models::ComplianceLevel::Excellent;
        if (score >= 80)
            return models::ComplianceLevel::Good;
        if (score >= 70)
            return models::ComplianceLevel::Qualified;
        return models::ComplianceLevel::Unqualified;
    }

    models::RiskLevel EvaluationService::evaluateRisk(double score)
    {
        if (score >= 70)
            return models::RiskLevel::High;
        if (score >= 40)
            return models::RiskLevel::Medium;
        return models::RiskLevel::Low;
    }

    std::optional<models::Evaluation> EvaluationService::getEvaluationByPeriod(int periodId)
    {
        return evaluationDao_->findByPeriodId(periodId);
    }

    void EvaluationService::updateTaskProgress(int taskId, int progress, const std::string& message)
    {
        auto taskOpt = taskDao_->findById(taskId);
        if (!taskOpt)
            return;

        auto task = *taskOpt;
        task.progress = progress;

        if (progress == 100)
        {
            task.status = models::TaskStatus::Completed;
            task.completed_at = std::chrono::system_clock::now();
        }
        else if (progress < 0)
        {
            task.status = models::TaskStatus::Failed;
            task.error_message = message;
        }
        else if (progress == 0)
        {
            task.status = models::TaskStatus::Running;
            task.started_at = std::chrono::system_clock::now();
        }

        taskDao_->update(task);
    }

}  // namespace soilwatercm::services
