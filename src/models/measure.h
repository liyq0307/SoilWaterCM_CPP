#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "common/enums.h"
#include "common/geo_types.h"

namespace soilwatercm::models
{

    /**
     * @brief 水保措施, 对应 measures 表
     *
     * 工程/植物/临时措施的图斑与状态。几何以 WKT 入库 (SRID 4326)。
     */
    struct Measure
    {
        int id = 0;
        MeasureType type = MeasureType::Engineering;
        std::string subtype;  ///< 具体类型, 如拦渣坝/绿化
        Geometry geometry;
        double length = 0.0;  ///< 线状措施长度
        double area = 0.0;    ///< 面状措施面积
        std::string status;   ///< 运行状态, 取 MeasureStatus 的名字
        std::string condition;  ///< 完好程度, 取 MeasureCondition 的名字
        std::optional<double> vegetation_coverage;  ///< 植物措施的覆盖度, 非植物措施为空
        int period_id = 0;
        std::chrono::system_clock::time_point created_at;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Measure, id, type, subtype, length, area, status, condition, vegetation_coverage,
                                       period_id)
    };

}  // namespace soilwatercm::models
