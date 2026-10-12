#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace soilwatercm::utils
{

    /**
     * @brief WKT <-> GeoJSON 转换工具
     *
     * 支持 POINT / POLYGON / MULTIPOLYGON, 2D/3D。
     */
    class GeoJsonUtils
    {
    public:
        /**
         * @brief WKT 字符串 -> GeoJSON geometry 对象
         * @param wkt 几何 WKT
         * @return geometry JSON; 解析失败返回 null
         */
        static nlohmann::json wktToGeoJson(const std::string& wkt);

        /// @brief GeoJSON geometry 对象 -> WKT 字符串; 不支持的类型返回空串
        static std::string geoJsonToWkt(const nlohmann::json& geometry);

        /// @brief 组装 GeoJSON FeatureCollection
        static nlohmann::json makeFeatureCollection(const nlohmann::json& features);

        /// @brief 组装 GeoJSON Feature
        static nlohmann::json makeFeature(const nlohmann::json& geometry, const nlohmann::json& properties);

        /**
         * @brief 球面多边形面积 (公顷)
         * @param wkt 几何 WKT (POLYGON/MULTIPOLYGON)
         * @return 面积; 解析失败返回 -1
         */
        static double areaHa(const std::string& wkt);
    };

}  // namespace soilwatercm::utils
