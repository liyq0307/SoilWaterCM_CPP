#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "common/enums.h"
#include "common/geo_types.h"

namespace soilwatercm::models
{

    /**
     * @brief 遥感/无人机影像, 对应 images 表
     *
     * 每个监测期可挂多景。质检状态与质检说明由质检流程回填。
     */
    struct Image
    {
        int id = 0;
        std::string image_code;
        ImageType type = ImageType::Optical;
        std::string sensor_type;  ///< 传感器, 如 Landsat-8 / Sentinel-1
        std::chrono::system_clock::time_point acquisition_time;
        Geometry extent;          ///< 影像覆盖范围
        std::string crs = "EPSG:4326";
        double resolution = 0.0;  ///< 地面分辨率 (米)
        std::string file_path;
        size_t file_size = 0;     ///< 文件大小 (字节)
        std::optional<double> cloud_cover;  ///< 云量百分比
        QualityStatus quality_status = QualityStatus::Pending;
        std::optional<std::string> quality_message;  ///< 质检说明
        int project_id = 0;
        int period_id = 0;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Image, id, image_code, type, sensor_type, acquisition_time, crs, resolution,
                                       file_path, file_size, cloud_cover, quality_status, quality_message, project_id,
                                       period_id)
    };

}  // namespace soilwatercm::models
