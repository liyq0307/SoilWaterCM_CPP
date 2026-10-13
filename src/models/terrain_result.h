#pragma once

#include <chrono>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace soilwatercm::models
{

    /**
     * @brief 地形/专项分析成果, 对应 terrain_results 表
     *
     * 统一承载地形指数、淹没、视域、InSAR 形变等以栅格为主的单值成果,
     * analysis 区分类型, stats 存 JSON 统计信息。
     */
    struct TerrainResult
    {
        int id = 0;
        int project_id = 0;
        std::string analysis;             ///< twi/tpi/tri/inundation/viewshed/insar 等
        std::string raster_path;          ///< 栅格成果路径
        std::string stats_json;           ///< 统计信息 JSON 文本
        std::chrono::system_clock::time_point created_at;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(TerrainResult, id, project_id, analysis, raster_path)
    };

}  // namespace soilwatercm::models
