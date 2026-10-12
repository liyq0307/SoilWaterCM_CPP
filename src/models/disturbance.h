#pragma once

#include <chrono>
#include <string>

#include "common/enums.h"
#include "common/geo_types.h"

namespace soilwatercm::models
{

    /**
     * @brief 扰动地块, 对应 disturbances 表
     *
     * 遥感提取的人为扰动图斑, 含面积、周长与提取置信度。
     */
    struct Disturbance
    {
        int id = 0;
        std::string disturbance_code;
        DisturbanceType type = DisturbanceType::Other;
        Geometry geometry;        ///< 图斑边界
        double area = 0.0;
        double perimeter = 0.0;
        double confidence = 0.0;  ///< 提取置信度 0-1
        int period_id = 0;
        std::chrono::system_clock::time_point created_at;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Disturbance, id, disturbance_code, type, area, perimeter, confidence, period_id)
    };

}  // namespace soilwatercm::models
