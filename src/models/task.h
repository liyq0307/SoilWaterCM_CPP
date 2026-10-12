#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "common/enums.h"
#include <nlohmann/json.hpp>

namespace soilwatercm::models
{

    /**
     * @brief 后台处理任务, 对应 tasks 表
     *
     * 影像预处理/要素提取/报告生成等异步作业, 带进度与重试计数。
     */
    struct Task
    {
        int id = 0;
        TaskType type = TaskType::Preprocess;
        TaskStatus status = TaskStatus::Pending;
        double progress = 0.0;  ///< 进度 0-100
        std::string params;     ///< 任务入参, JSON 文本
        std::optional<std::string> result_path;     ///< 成果文件路径, 未完成为空
        std::optional<std::string> error_message;   ///< 失败原因, 成功为空
        std::chrono::system_clock::time_point submitted_at;
        std::optional<std::chrono::system_clock::time_point> started_at;
        std::optional<std::chrono::system_clock::time_point> completed_at;
        int retry_count = 0;
        int project_id = 0;
        int period_id = 0;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Task, id, type, status, progress, params, result_path, error_message,
                                       submitted_at, started_at, completed_at, retry_count, project_id, period_id)
    };

}  // namespace soilwatercm::models
