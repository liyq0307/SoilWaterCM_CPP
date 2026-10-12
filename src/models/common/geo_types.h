#pragma once

#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace soilwatercm::models
{

    /**
     * @brief 平面坐标点
     *
     * 经纬度或投影坐标, x 为经度/横坐标, y 为纬度/纵坐标。
     */
    struct Point
    {
        double x;
        double y;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Point, x, y)
    };

    /**
     * @brief GeoJSON 多边形
     *
     * 前端传来的边界数据用这种格式。coordinates 是环列表,
     * 第一个环为外边界, 其余为洞; toWkt 转成入库用的 WKT。
     */
    struct Polygon
    {
        std::string type = "Polygon";
        std::vector<std::vector<Point>> coordinates;

        /**
         * @brief 转为 WKT 文本 (只取外环), 供 PostGIS ST_GeomFromText 入库
         * @return POLYGON 文本
         */
        std::string toWkt() const
        {
            std::ostringstream oss;
            oss << "POLYGON((";

            if (!coordinates.empty() && !coordinates[0].empty())
            {
                for (size_t i = 0; i < coordinates[0].size(); ++i)
                {
                    if (i > 0)
                        oss << ", ";
                    oss << coordinates[0][i].x << " " << coordinates[0][i].y;
                }
            }

            oss << "))";
            return oss.str();
        }

        /**
         * @brief 从 GeoJSON 对象解析
         * @param json GeoJSON 对象
         * @return 多边形
         */
        static Polygon fromGeoJson(const nlohmann::json& json)
        {
            Polygon poly;
            if (json.contains("type"))
            {
                poly.type = json["type"];
            }
            if (json.contains("coordinates") && json["coordinates"].is_array())
            {
                for (const auto& ring : json["coordinates"])
                {
                    std::vector<Point> ringPoints;
                    for (const auto& coord : ring)
                    {
                        Point p;
                        p.x = coord[0].get<double>();
                        p.y = coord[1].get<double>();
                        ringPoints.push_back(p);
                    }
                    poly.coordinates.push_back(ringPoints);
                }
            }
            return poly;
        }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Polygon, type, coordinates)
    };

    /**
     * @brief 几何对象在库里的样子
     *
     * PostGIS geometry 列读出后就是 WKT 文本, 前端交互时再转 GeoJSON。
     * srid 默认 EPSG:4326 (经纬度)。
     */
    struct Geometry
    {
        std::string wkt;
        std::string srid = "EPSG:4326";
        bool is_valid = false;

        /// wkt 非空即视为有效
        bool isValid() const { return !wkt.empty(); }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Geometry, wkt, srid)
    };

    /**
     * @brief 统一 API 响应包装
     *
     * code + message + 可选数据。
     */
    template <typename T>
    struct ApiResponse
    {
        int code;
        std::string message;
        std::optional<T> data;

        static ApiResponse success(const T& data) { return {200, "success", data}; }

        static ApiResponse error(int code, const std::string& message) { return {code, message, std::nullopt}; }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(ApiResponse, code, message, data)
    };

    /**
     * @brief 分页列表响应
     *
     * items 为当前页记录, total 为总条数, page/size 为页码与每页条数。
     */
    template <typename T>
    struct PagedResponse
    {
        std::vector<T> items;
        int total;
        int page;
        int size;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(PagedResponse, items, total, page, size)
    };

}  // namespace soilwatercm::models
