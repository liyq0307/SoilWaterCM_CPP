#pragma once

#include <chrono>
#include <map>
#include <string>
#include <vector>

#include "common/enums.h"
#include <nlohmann/json.hpp>

namespace soilwatercm::models
{

    /**
     * @brief 合规评价结果, 对应 evaluations 表
     *
     * 每期一条, 汇总各指标得分, 给出合规分/等级与风险分/等级。
     */
    struct Evaluation
    {
        int id = 0;
        double compliance_score = 0.0;
        ComplianceLevel compliance_level = ComplianceLevel::Qualified;
        double risk_score = 0.0;
        RiskLevel risk_level = RiskLevel::Medium;
        std::map<std::string, double> indicator_weights;  ///< 指标代码 -> 权重
        std::map<std::string, double> indicator_values;   ///< 指标代码 -> 实测值
        int period_id = 0;
        std::chrono::system_clock::time_point created_at;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Evaluation, id, compliance_score, compliance_level, risk_score, risk_level,
                                       indicator_weights, indicator_values, period_id)
    };

}  // namespace soilwatercm::models
