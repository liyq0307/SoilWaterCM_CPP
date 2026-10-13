#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "common/geo_types.h"
#include <nlohmann/json.hpp>

namespace soilwatercm::models
{

    /**
     * @brief 弃渣场图斑, 对应 spoil_dumps 表
     *
     * 与扰动图斑的区别: 需结构化存储堆渣方量/堆高/稳定性/合规性,
     * 检出的违规或失稳弃渣另行生成异常记录。
     */
    struct SpoilDump
    {
        int id = 0;
        int project_id = 0;
        int period_id = 0;
        std::optional<int> image_id;
        Geometry geom;                       ///< MULTIPOLYGON, SRID 4326
        double area_m2 = 0.0;
        std::optional<double> volume_m3;     ///< 堆渣方量
        std::optional<double> max_height_m;  ///< 最大堆高
        std::optional<double> mean_height_m; ///< 平均堆高
        std::optional<double> mean_slope_deg;
        std::optional<bool> in_approved_area;
        std::optional<bool> has_protection;
        std::string protection_types;
        std::string stability_level = "low_risk";  ///< stable/low_risk/medium_risk/high_risk/critical
        std::optional<double> risk_score;
        std::string compliance = "pending";        ///< compliant/non_compliant/pending
        std::string detect_method = "dem_diff";
        std::optional<double> confidence;
        std::string note;
        std::chrono::system_clock::time_point created_at;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(SpoilDump, id, project_id, period_id, area_m2, stability_level, compliance)
    };

}  // namespace soilwatercm::models
