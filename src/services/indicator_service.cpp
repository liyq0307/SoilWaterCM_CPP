#include "indicator_service.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "../common/exceptions.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{

    namespace
    {
        // 随机一致性指标 RI 表 (对齐 Python ahp.py)
        double riValue(int n)
        {
            static const std::map<int, double> ri = {
                {1, 0.0},  {2, 0.0},   {3, 0.58},  {4, 0.90},  {5, 1.12},  {6, 1.24},  {7, 1.32},  {8, 1.41},
                {9, 1.45}, {10, 1.49}, {11, 1.51}, {12, 1.54}, {13, 1.56}, {14, 1.58}, {15, 1.59},
            };
            auto it = ri.find(n);
            if (it != ri.end())
                return it->second;
            return 1.6;
        }

        // 幂迭代求最大特征向量 (替代 numpy.linalg.eig)
        // 收敛后按分量归一化得权重, 由 M*w 与 w 的比值估计 lambda_max
        void powerIteration(const std::vector<std::vector<double>>& m, std::vector<double>& weights, double& lambdaMax)
        {
            const int n = static_cast<int>(m.size());
            std::vector<double> v(n, 1.0 / n);
            lambdaMax = static_cast<double>(n);

            constexpr int kMaxIter = 1000;
            constexpr double kTol = 1e-12;

            for (int iter = 0; iter < kMaxIter; ++iter)
            {
                std::vector<double> mv(n, 0.0);
                for (int i = 0; i < n; ++i)
                {
                    double s = 0.0;
                    for (int j = 0; j < n; ++j)
                        s += m[i][j] * v[j];
                    mv[i] = s;
                }

                double sum = std::accumulate(mv.begin(), mv.end(), 0.0);
                if (sum <= 0.0 || !std::isfinite(sum))
                    throw common::ValidationException("判断矩阵无效, 无法收敛");

                std::vector<double> next(n);
                double diff = 0.0;
                for (int i = 0; i < n; ++i)
                {
                    next[i] = mv[i] / sum;
                    diff = std::max(diff, std::abs(next[i] - v[i]));
                }
                v = std::move(next);

                if (diff < kTol)
                    break;
            }

            // lambda_max 由 Rayleigh 商估计: sum((M*v)_i / v_i) / n
            double lam = 0.0;
            for (int i = 0; i < n; ++i)
            {
                double s = 0.0;
                for (int j = 0; j < n; ++j)
                    s += m[i][j] * v[j];
                lam += s / v[i];
            }
            lambdaMax = lam / n;
            weights = v;
        }
    }  // namespace

    IndicatorService::IndicatorService(std::shared_ptr<dao::IndicatorDao> indicatorDao)
        : indicatorDao_(std::move(indicatorDao))
    {
    }

    std::vector<models::Indicator> IndicatorService::listIndicators(const std::string& dimension)
    {
        if (!dimension.empty() && !models::validIndicatorDimensions().count(dimension))
            throw common::ValidationException("无效指标维度: " + dimension);
        return indicatorDao_->listActive(dimension);
    }

    std::optional<models::Indicator> IndicatorService::getIndicator(int id)
    {
        return indicatorDao_->findById(id);
    }

    std::optional<models::Indicator> IndicatorService::getIndicatorByCode(const std::string& code)
    {
        return indicatorDao_->findByCode(code);
    }

    models::Indicator IndicatorService::setWeight(int id, double weight)
    {
        auto existing = indicatorDao_->findById(id);
        if (!existing)
            throw common::NotFoundException("指标不存在");
        if (weight < 0.0 || weight > 1.0)
            throw common::ValidationException("权重必须在 [0, 1] 范围内");

        indicatorDao_->updateWeight(id, weight);
        return *indicatorDao_->findById(id);
    }

    models::Indicator IndicatorService::setThresholds(int id, std::optional<double> good, std::optional<double> warn,
                                                      std::optional<double> bad)
    {
        auto existing = indicatorDao_->findById(id);
        if (!existing)
            throw common::NotFoundException("指标不存在");

        indicatorDao_->updateThresholds(id, good, warn, bad);
        return *indicatorDao_->findById(id);
    }

    std::vector<models::IndicatorValue> IndicatorService::listValues(int projectId, std::optional<int> periodId,
                                                                     std::optional<int> indicatorId)
    {
        return indicatorDao_->listValues(projectId, periodId, indicatorId);
    }

    AhpResult IndicatorService::ahpWeights(const std::vector<std::vector<double>>& judgmentMatrix)
    {
        AhpResult result;
        const int n = static_cast<int>(judgmentMatrix.size());
        if (n == 0)
            throw common::ValidationException("判断矩阵不能为空");

        for (const auto& row : judgmentMatrix)
        {
            if (static_cast<int>(row.size()) != n)
                throw common::ValidationException("判断矩阵必须为方阵");
            for (double v : row)
            {
                if (v <= 0.0 || !std::isfinite(v))
                    throw common::ValidationException("判断矩阵元素必须为正数");
            }
        }

        if (n == 1)
        {
            result.weights = {1.0};
            result.cr = 0.0;
            result.consistent = true;
            return result;
        }

        double lambdaMax = 0.0;
        powerIteration(judgmentMatrix, result.weights, lambdaMax);

        // 一致性指标 CI = (lambda_max - n) / (n - 1)
        double ci = (lambdaMax - n) / (n - 1);
        double ri = riValue(n);
        result.cr = (ri > 0.0) ? ci / ri : 0.0;
        result.consistent = result.cr < 0.1;
        return result;
    }

}  // namespace soilwatercm::services
