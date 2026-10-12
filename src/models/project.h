#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "common/enums.h"
#include "common/geo_types.h"
#include <nlohmann/json.hpp>

namespace soilwatercm::models
{

    /**
     * @brief 监测项目, 对应 projects 表
     *
     * 一个矿山/工程的水保监测单元, 其余数据都挂在项目下。
     */
    struct Project
    {
        int id = 0;
        std::string code;
        std::string name;
        Geometry responsibility_area;  ///< 防治责任范围边界
        double center_lon = 0.0;
        double center_lat = 0.0;
        ProjectStatus status = ProjectStatus::Active;
        std::chrono::system_clock::time_point created_at;
        std::chrono::system_clock::time_point updated_at;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Project, id, code, name, center_lon, center_lat, status)
    };

}  // namespace soilwatercm::models
