#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "common/geo_types.h"

namespace soilwatercm::models
{

    /**
     * @brief 三维场景底图影像, 对应 basemap_images 表
     *
     * 存上传的正射影像等底图及其显示参数, 供三维场景加载。
     * 几何范围以 WKT 入库 (SRID 4326)。
     */
    struct BasemapImage
    {
        int id = 0;
        int project_id = 0;
        std::string name;
        std::string file_path;
        std::optional<double> file_size_mb;  ///< 文件大小 (MB), 未统计为空
        std::optional<int> width_px;         ///< 影像宽度 (像素)
        std::optional<int> height_px;        ///< 影像高度 (像素)
        std::string crs = "EPSG:4326";
        Geometry footprint;                  ///< POLYGON, SRID 4326
        std::optional<int> band_count;       ///< 波段数
        std::optional<double> stretch_min;   ///< 显示拉伸的 2% 百分位值
        std::optional<double> stretch_max;   ///< 显示拉伸的 98% 百分位值
        std::optional<std::string> wms_layer_name;  ///< 发布 WMS 服务后的图层名, 未发布为空
        std::string note;
        std::chrono::system_clock::time_point created_at;
    };

}  // namespace soilwatercm::models
