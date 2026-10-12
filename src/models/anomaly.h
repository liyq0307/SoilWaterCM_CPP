#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "common/enums.h"
#include "common/geo_types.h"

namespace soilwatercm::models
{

    /**
     * @brief 异常事件, 对应 anomalies 表
     *
     * 自动识别或人工上报的水保问题, 带位置、严重程度和处置流程状态。
     */
    struct Anomaly
    {
        int id = 0;
        AnomalyType type = AnomalyType::DisturbanceViolation;
        Geometry geometry;  ///< 异常位置
        Severity severity = Severity::Medium;
        std::string description;
        DisposalStatus status = DisposalStatus::Pending;
        std::optional<std::string> assignee;  ///< 处置责任人, 未派单为空
        std::optional<std::string> measures;  ///< 处置措施说明
        std::chrono::system_clock::time_point detected_at;
        std::optional<std::chrono::system_clock::time_point> resolved_at;  ///< 处置完成时间, 未完成为空
        int project_id = 0;
        int period_id = 0;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Anomaly, id, type, severity, description, status, assignee, measures,
                                       detected_at, resolved_at, project_id, period_id)
    };

}  // namespace soilwatercm::models
