#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../dao/indicator_dao.h"
#include "../models/indicator.h"

namespace soilwatercm::services
{

    /** @brief AHP 权重计算结果 */
    struct AhpResult
    {
        std::vector<double> weights;  ///< 归一化权重向量
        double cr = 0.0;              ///< 一致性比率
        bool consistent = true;       ///< CR < 0.1 视为通过一致性检验
    };

    /**
     * @brief 评价指标字典与取值服务
     *
     * 维护指标定义 (权重、阈值), 查询分期指标值, 并提供 AHP 层次分析法
     * 权重计算 (幂迭代求判断矩阵最大特征向量)。
     */
    class IndicatorService
    {
    public:
        /**
         * @brief 构造, 注入指标 DAO
         * @param indicatorDao 指标 DAO
         */
        explicit IndicatorService(std::shared_ptr<dao::IndicatorDao> indicatorDao);

        // ---- Indicator ----

        /**
         * @brief 列出启用指标
         * @param dimension 指标维度; 为空则不过滤
         * @return 指标列表
         * @throw ValidationException 维度无效时
         */
        std::vector<models::Indicator> listIndicators(const std::string& dimension);

        /**
         * @brief 按 id 查单个指标
         * @param id 指标主键
         * @return 指标; 不存在返回 nullopt
         */
        std::optional<models::Indicator> getIndicator(int id);

        /**
         * @brief 按编码查单个指标
         * @param code 指标编码
         * @return 指标; 不存在返回 nullopt
         */
        std::optional<models::Indicator> getIndicatorByCode(const std::string& code);

        /**
         * @brief 设置指标权重
         * @param id 指标主键
         * @param weight 权重, 须在 [0, 1]
         * @return 更新后指标
         * @throw NotFoundException 指标不存在时
         * @throw ValidationException 权重越界时
         */
        models::Indicator setWeight(int id, double weight);

        /**
         * @brief 设置指标阈值 (良好/警告/不良)
         * @param id 指标主键
         * @param good 良好阈值; nullopt 表示不改
         * @param warn 警告阈值
         * @param bad 不良阈值
         * @return 更新后指标
         * @throw NotFoundException 指标不存在时
         */
        models::Indicator setThresholds(int id, std::optional<double> good, std::optional<double> warn,
                                        std::optional<double> bad);

        // ---- IndicatorValue ----

        /**
         * @brief 查指标取值
         * @param projectId 项目 id
         * @param periodId 期次 id; 为空则不过滤
         * @param indicatorId 指标 id; 为空则不过滤
         * @return 指标值列表
         */
        std::vector<models::IndicatorValue> listValues(int projectId, std::optional<int> periodId,
                                                       std::optional<int> indicatorId);

        // ---- AHP 层次分析法 ----

        /**
         * @brief AHP 权重计算
         *
         * 幂迭代求最大特征向量得归一化权重; 由 lambda_max 反算 CI 再除以 RI 得 CR。
         * @param judgmentMatrix n x n 正互反判断矩阵
         * @return 权重向量 + 一致性比率 + 是否通过检验
         * @throw ValidationException 矩阵为空 / 非方阵 / 元素非正 / 幂迭代无法收敛时
         */
        static AhpResult ahpWeights(const std::vector<std::vector<double>>& judgmentMatrix);

    private:
        std::shared_ptr<dao::IndicatorDao> indicatorDao_;
    };

}  // namespace soilwatercm::services
