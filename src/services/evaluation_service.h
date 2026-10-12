#pragma once

#include <future>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../dao/evaluation_dao.h"
#include "../dao/task_dao.h"
#include "../models/evaluation.h"

namespace soilwatercm::services
{

    /** @brief 综合评价入参 */
    struct EvaluationParams
    {
        int period_id;                                          ///< 期次 id
        std::map<std::string, double> indicator_values;         ///< 指标编码 -> 指标值
        std::optional<std::map<std::string, double>> weights;   ///< 外部指定权重; 为空则自动计算
        std::string method = "ahp";                             ///< 权重方法 (ahp/entropy)
    };

    /** @brief 综合评价结果 */
    struct EvaluationResult
    {
        bool success = false;                                       ///< 是否成功
        std::string message;                                        ///< 结果说明/失败原因
        double compliance_score = 0.0;                              ///< 达标得分
        models::ComplianceLevel compliance_level = models::ComplianceLevel::Qualified;  ///< 达标等级
        double risk_score = 0.0;                                    ///< 风险得分
        models::RiskLevel risk_level = models::RiskLevel::Low;      ///< 风险等级
        std::map<std::string, double> weights;                      ///< 实际使用权重
        std::map<std::string, double> indicator_values;             ///< 参与评价的指标值
        std::optional<int> task_id;                                 ///< 关联任务 id
    };

    /**
     * @brief 综合评价服务
     *
     * 对各指标做加权求和得达标得分, 反推风险得分并分级入库;
     * 权重可外部指定, 也可用 AHP 主观法或熵权客观法计算。
     */
    class EvaluationService
    {
    public:
        /**
         * @brief 构造, 注入评价与任务 DAO
         * @param evaluationDao 评价结果 DAO (入库)
         * @param taskDao 任务 DAO (进度回写)
         */
        explicit EvaluationService(std::shared_ptr<dao::EvaluationDao> evaluationDao,
                                   std::shared_ptr<dao::TaskDao> taskDao);

        ~EvaluationService() = default;

        /**
         * @brief 异步提交评价任务
         * @param params 评价入参
         * @return 结果 future (建任务记录后后台执行)
         */
        std::future<EvaluationResult> submitEvaluationTask(const EvaluationParams& params);

        /**
         * @brief 评价主流程
         *
         * 加权求和得达标分 (90/80/70 分档定级), 风险分 = 100 - 达标分, 结果入库。
         * @param params 评价入参
         * @param taskId 关联任务 id
         * @return 评价结果
         */
        EvaluationResult evaluate(const EvaluationParams& params, int taskId);

        /**
         * @brief AHP 权重计算
         *
         * 对判断矩阵做幂迭代 (反复乘矩阵并归一化), 收敛后取最大特征向量。
         * @param comparisonMatrix 指标两两比较矩阵 (嵌套 map)
         * @return 指标编码 -> 权重
         */
        std::map<std::string, double> calculateAhpWeights(
            const std::map<std::string, std::map<std::string, double>>& comparisonMatrix);

        /**
         * @brief 一致性检验
         * @param comparisonMatrix 判断矩阵
         * @param weights 权重向量
         * @return CR = CI/RI < 0.1 视为通过
         */
        bool checkConsistency(const std::map<std::string, std::map<std::string, double>>& comparisonMatrix,
                              const std::map<std::string, double>& weights);

        /**
         * @brief 熵权法权重
         *
         * 指标数据 min-max 归一化 -> 算信息熵 -> 差异系数 (1-熵) 归一化得权重。
         * @param indicatorData 指标编码 -> 样本值序列
         * @return 指标编码 -> 权重; 数据为空时返回占位的 4 指标均等权重
         */
        std::map<std::string, double> calculateEntropyWeights(
            const std::map<std::string, std::vector<double>>& indicatorData);

        /**
         * @brief 组合权重
         * @param ahpWeights AHP 权重
         * @param entropyWeights 熵权
         * @param alpha 主观权重占比, 默认 0.5
         * @return alpha * AHP + (1 - alpha) * 熵权
         */
        std::map<std::string, double> combineWeights(const std::map<std::string, double>& ahpWeights,
                                                     const std::map<std::string, double>& entropyWeights,
                                                     double alpha = 0.5);

        /**
         * @brief 达标等级判定
         * @param score 达标得分
         * @return >=90 优秀, >=80 良好, >=70 合格, 否则不合格
         */
        models::ComplianceLevel evaluateCompliance(double score);

        /**
         * @brief 风险等级判定
         * @param score 风险得分
         * @return >=70 高, >=40 中, 否则低
         */
        models::RiskLevel evaluateRisk(double score);

        /**
         * @brief 按期次查评价结果
         * @param periodId 期次 id
         * @return 评价记录; 不存在返回 nullopt
         */
        std::optional<models::Evaluation> getEvaluationByPeriod(int periodId);

    private:
        std::shared_ptr<dao::EvaluationDao> evaluationDao_;
        std::shared_ptr<dao::TaskDao> taskDao_;

        double calculateConsistencyRatio(const std::map<std::string, std::map<std::string, double>>& matrix,
                                         const std::map<std::string, double>& weights);

        void updateTaskProgress(int taskId, int progress, const std::string& message);
    };

}  // namespace soilwatercm::services
