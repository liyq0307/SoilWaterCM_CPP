#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "common/geo_types.h"

namespace soilwatercm::models
{

    /// 水土保持监测标准 12 防治分区 (代码 -> 中文名)
    inline const std::map<std::string, std::string>& zoneCodes()
    {
        static const std::map<std::string, std::string> codes = {
            {"open_pit", "露天采场区"},
            {"waste_dump", "排土场区"},
            {"tailings_pond", "尾矿库区"},
            {"industrial_area", "工业场地区"},
            {"pipeline_system", "精尾矿输送管线及回水系统区"},
            {"road_area", "矿山道路工程区"},
            {"crushing_transport", "破碎运输廊道区"},
            {"auxiliary_area", "公共辅助工程区"},
            {"construction_living", "施工生产生活区"},
            {"construction_road", "施工便道区"},
            {"topsoil_stack", "表土堆场区"},
            {"temp_soil_stack", "临时堆土场区（新增）"},
        };
        return codes;
    }

    /// 中文名 -> 代码
    inline const std::map<std::string, std::string>& zoneNameToCode()
    {
        static const std::map<std::string, std::string> mapping = []()
        {
            std::map<std::string, std::string> m;
            for (const auto& [code, name] : zoneCodes())
                m[name] = code;
            return m;
        }();
        return mapping;
    }

    /// 合法分区代码集合, 由 zoneCodes() 派生
    inline const std::set<std::string>& validZoneCodes()
    {
        static const std::set<std::string> codes = []()
        {
            std::set<std::string> s;
            for (const auto& [code, _] : zoneCodes())
                s.insert(code);
            return s;
        }();
        return codes;
    }

    /**
     * @brief 分区统计, 对应 zone_stats 表 (设计/批复数据)
     *
     * 存各分区各期次的设计面积、设计措施与落实进度, 用于报告里分区级表格。
     */
    struct ZoneStats
    {
        int id = 0;
        int project_id = 0;
        std::optional<int> period_id;  ///< 所属监测期; 空表示项目级设计数据
        std::string zone;              ///< 防治分区代码, 见 zoneCodes()
        std::optional<double> designed_area_ha;  ///< 设计面积 (公顷)
        std::string designed_measures;           ///< 设计措施内容, JSON 文本
        std::string progress_status;             ///< 措施落实进度
        std::string note;
        std::chrono::system_clock::time_point created_at;
        std::chrono::system_clock::time_point updated_at;
    };

    /**
     * @brief 分区边界, 对应 zone_boundaries 表
     *
     * 项目级数据, 每个分区一条多边形。
     */
    struct ZoneBoundary
    {
        int id = 0;
        int project_id = 0;
        std::string zone;  ///< 防治分区代码, 见 zoneCodes()
        Geometry geom;     ///< MULTIPOLYGON, SRID 4326
        std::optional<double> area_ha;  ///< 分区面积 (公顷)
        std::string note;
        std::chrono::system_clock::time_point created_at;
    };

}  // namespace soilwatercm::models
