#pragma once

#include <chrono>
#include <string>

#include "common/enums.h"
#include <nlohmann/json.hpp>

namespace soilwatercm::models
{

    /**
     * @brief 土壤侵蚀定量评价成果, 对应 erosion_results 表
     *
     * USLE 模型输出, 每期一条。分级面积按侵蚀强度六级
     * (微度/轻度/中度/强烈/极强烈/剧烈) 统计。
     */
    struct ErosionResult
    {
        int id = 0;
        std::string modulus_raster_path;  ///< 侵蚀模数栅格
        std::string class_raster_path;    ///< 侵蚀强度分级栅格
        double mean_modulus = 0.0;        ///< 平均侵蚀模数 t/(ha·a)
        double slight_area = 0.0;         ///< 微度面积
        double light_area = 0.0;          ///< 轻度面积
        double moderate_area = 0.0;       ///< 中度面积
        double strong_area = 0.0;         ///< 强烈面积
        double very_strong_area = 0.0;    ///< 极强烈面积
        double severe_area = 0.0;         ///< 剧烈面积
        int period_id = 0;
        std::chrono::system_clock::time_point created_at;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(ErosionResult, id, modulus_raster_path, class_raster_path, mean_modulus,
                                       slight_area, light_area, moderate_area, strong_area, very_strong_area,
                                       severe_area, period_id)
    };

}  // namespace soilwatercm::models
