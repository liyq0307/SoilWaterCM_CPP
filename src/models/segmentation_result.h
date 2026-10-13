#pragma once

#include <chrono>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace soilwatercm::models
{

    /**
     * @brief 影像分割成果, 对应 segmentation_results 表
     *
     * MeanShift/Watershed/SLIC 等分割输出。
     */
    struct SegmentationResult
    {
        int id = 0;
        std::optional<int> project_id;
        std::optional<int> period_id;
        int image_id = 0;
        std::string method;               ///< mean_shift/watershed/superpixel/lsms
        std::optional<int> n_segments;
        std::string raster_path;          ///< 分割标签栅格路径
        std::chrono::system_clock::time_point created_at;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(SegmentationResult, id, image_id, method, raster_path)
    };

}  // namespace soilwatercm::models
