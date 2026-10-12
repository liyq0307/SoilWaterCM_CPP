#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "common/geo_types.h"

namespace soilwatercm::models
{

    /**
     * @brief 地面验证记录, 对应 ground_truths 表
     *
     * 外业样地实测数据, 并可关联遥感提取结果做精度评估。
     * 样地类型: erosion_gully/veg_quadrat/measure_dimension/disturbance_boundary/other。
     */
    struct GroundTruth
    {
        int id = 0;
        int project_id = 0;
        std::optional<int> period_id;  ///< 所属监测期; 空表示不挂期次

        std::string sample_type;  ///< 样地类型
        Geometry location;        ///< POINT, SRID 4326

        std::chrono::system_clock::time_point survey_date;
        std::string surveyor;

        std::optional<double> measured_value;  ///< 实测值
        std::string measured_unit;             ///< 实测值单位

        std::string target_table;  ///< 关联的遥感结果表名, 如 disturbances; 空表示不做精度对比
        std::optional<int> target_id;
        std::optional<double> rs_value;     ///< 遥感提取值
        std::optional<double> error;        ///< 实测与遥感值的偏差
        std::optional<bool> accuracy_pass;  ///< 偏差是否在允许范围内

        std::string photo_paths;  ///< 现场照片路径, JSON list 文本
        std::string note;

        std::chrono::system_clock::time_point created_at;
    };

}  // namespace soilwatercm::models
