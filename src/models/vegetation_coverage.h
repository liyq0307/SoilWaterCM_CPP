#pragma once

#include <chrono>
#include <string>

#include "common/enums.h"
#include <nlohmann/json.hpp>

namespace soilwatercm::models
{

    /**
     * @brief 植被覆盖度成果, 对应 vegetation_coverages 表
     *
     * 每期一条, 记录 FVC 反演统计。分级面积按覆盖度五级统计。
     */
    struct VegetationCoverage
    {
        int id = 0;
        std::string fvc_raster_path;  ///< FVC 栅格成果路径
        double mean_fvc = 0.0;        ///< 覆盖度均值
        double low_coverage_area = 0.0;
        double medium_low_area = 0.0;
        double medium_area = 0.0;
        double medium_high_area = 0.0;
        double high_coverage_area = 0.0;
        int period_id = 0;
        std::chrono::system_clock::time_point created_at;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(VegetationCoverage, id, fvc_raster_path, mean_fvc, low_coverage_area,
                                       medium_low_area, medium_area, medium_high_area, high_coverage_area, period_id)
    };

}  // namespace soilwatercm::models
