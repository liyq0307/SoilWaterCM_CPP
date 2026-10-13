#pragma once

#include <chrono>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace soilwatercm::models
{

    /**
     * @brief 土地利用分类成果, 对应 landuse_results 表
     *
     * RF/SVM/最大似然/K-means/ISODATA 等分类输出,
     * class_areas_json 存各类面积统计。
     */
    struct LandUseResult
    {
        int id = 0;
        int project_id = 0;
        std::optional<int> period_id;
        std::optional<int> image_id;
        std::string method;               ///< random_forest/svm/mlc/kmeans/isodata 等
        std::optional<int> n_classes;
        std::string raster_path;          ///< 分类栅格路径
        std::string class_areas_json;     ///< 各类面积统计 JSON 文本
        std::chrono::system_clock::time_point created_at;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(LandUseResult, id, project_id, method, raster_path)
    };

}  // namespace soilwatercm::models
