#pragma once

#include <chrono>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace soilwatercm::models
{

    /**
     * @brief 训练模型注册, 对应 trained_models 表
     *
     * 独立于任务记录维护模型生命周期, 删除任务不删除注册信息;
     * model_path 为当前机器上的绝对路径。
     */
    struct TrainedModel
    {
        int id = 0;
        int project_id = 0;
        std::optional<int> period_id;
        std::optional<int> image_id;
        std::optional<long long> source_task_id;
        std::string method;
        std::string model_name;
        std::string model_path;
        std::string status = "available";   ///< available/archived
        std::string metrics_json;
        std::optional<long long> file_size;
        std::optional<std::string> file_sha256;
        std::chrono::system_clock::time_point created_at;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(TrainedModel, id, project_id, method, model_name, model_path, status)
    };

}  // namespace soilwatercm::models
