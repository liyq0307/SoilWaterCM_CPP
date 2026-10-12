#pragma once

#include <chrono>
#include <string>

#include "common/enums.h"
#include <nlohmann/json.hpp>

namespace soilwatercm::models
{

    /**
     * @brief 监测期, 对应 monitoring_periods 表
     *
     * 项目下按时间划分的监测单元, 影像和各类成果都挂到期次上。
     */
    struct MonitoringPeriod
    {
        int id = 0;
        std::string period_code;
        PeriodType type = PeriodType::Monthly;
        std::chrono::system_clock::time_point start_date;
        std::chrono::system_clock::time_point end_date;
        ProjectStatus status = ProjectStatus::Active;  ///< 复用项目状态: 进行中/已完成/已归档
        int project_id = 0;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(MonitoringPeriod, id, period_code, type, start_date, end_date, status,
                                       project_id)
    };

}  // namespace soilwatercm::models
