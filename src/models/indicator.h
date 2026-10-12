#pragma once

#include <chrono>
#include <optional>
#include <set>
#include <string>

namespace soilwatercm::models
{

    /// 指标维度 (对齐 Python: disturbance/vegetation/erosion/measure/geo_hazard/compliance)
    inline const std::set<std::string>& validIndicatorDimensions()
    {
        static const std::set<std::string> dims = {
            "disturbance", "vegetation", "erosion", "measure", "geo_hazard", "compliance",
        };
        return dims;
    }

    /// 指标方向 (higher_better / lower_better)
    inline const std::set<std::string>& validIndicatorDirections()
    {
        static const std::set<std::string> dirs = {
            "higher_better",
            "lower_better",
        };
        return dirs;
    }

    /// 评价等级 (good / warn / bad)
    inline const std::set<std::string>& validIndicatorLevels()
    {
        static const std::set<std::string> levels = {
            "good",
            "warn",
            "bad",
        };
        return levels;
    }

    /**
     * @brief 指标定义, 对应 indicators 表 (主数据)
     *
     * 描述一个监测指标的维度、单位、权重与三档评价阈值。
     */
    struct Indicator
    {
        int id = 0;
        std::string code;
        std::string name;
        std::string dimension;  ///< 所属维度, 见 validIndicatorDimensions
        std::optional<std::string> unit;
        std::optional<std::string> description;

        double weight = 0.0;  ///< 权重 (AHP/熵权法)

        /// 三档评价阈值, 配合 direction 判断: 达到 good 档记优, 越过 bad 档记差, 中间记 warn
        std::optional<double> threshold_good;
        std::optional<double> threshold_warn;
        std::optional<double> threshold_bad;

        std::string direction = "lower_better";  ///< 越大越好还是越小越好
        int is_active = 1;                       ///< 是否启用, 停用后不参与评价
    };

    /**
     * @brief 每期实测指标值, 对应 indicator_values 表
     *
     * 记录某项目某期某个指标的原始值、归一化值与得分。
     */
    struct IndicatorValue
    {
        int id = 0;
        int project_id = 0;
        int period_id = 0;
        int indicator_id = 0;

        double raw_value = 0.0;
        std::optional<double> normalized_value;  ///< 归一化值 0-1
        std::optional<double> score;             ///< 加权得分
        std::string level;                       ///< 评价等级 good/warn/bad

        std::chrono::system_clock::time_point created_at;
    };

}  // namespace soilwatercm::models
