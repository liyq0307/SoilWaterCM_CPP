#pragma once

#include <string>
#include <vector>

namespace soilwatercm::infrastructure::gdal
{

    /// 平面坐标点
    struct Point
    {
        double x;
        double y;

        Point(double x_ = 0.0, double y_ = 0.0) : x(x_), y(y_) {}
    };

    /**
     * @brief 纯空间计算工具
     *
     * 距离、投影换算、WKT 几何关系判断与叠加, 全部静态方法。
     * 几何入参均为 WKT, 解析失败时关系判断返回 false、运算返回空串。
     */
    class SpatialOps
    {
    public:
        SpatialOps();
        ~SpatialOps() = default;

        /// @brief 平面欧氏距离
        static double distance(const Point& p1, const Point& p2);

        /// @brief 平面欧氏距离
        static double distance(double x1, double y1, double x2, double y2);

        /**
         * @brief 经纬球面距离 (Haversine 公式, 地球半径按 6371km)
         * @param lon1 点1经度
         * @param lat1 点1纬度
         * @param lon2 点2经度
         * @param lat2 点2纬度
         * @return 距离 (米)
         */
        static double haversineDistance(double lon1, double lat1, double lon2, double lat2);

        /// @brief 经纬度投影到目标坐标系
        static Point project(double lon, double lat, const std::string& target_srs);

        /// @brief 投影坐标反算回经纬度
        static Point inverseProject(double x, double y, const std::string& source_srs);

        /// @brief 两几何是否相交
        static bool intersects(const std::string& geom1_wkt, const std::string& geom2_wkt);

        /// @brief 几何1是否包含几何2
        static bool contains(const std::string& geom1_wkt, const std::string& geom2_wkt);

        /// @brief 几何1是否在几何2内部
        static bool within(const std::string& geom1_wkt, const std::string& geom2_wkt);

        /// @brief 两几何是否相切
        static bool touches(const std::string& geom1_wkt, const std::string& geom2_wkt);

        /// @brief 交集几何 WKT
        static std::string intersection(const std::string& geom1_wkt, const std::string& geom2_wkt);

        /// @brief 并集几何 WKT
        static std::string union_(const std::string& geom1_wkt, const std::string& geom2_wkt);

        /// @brief 差集几何 WKT (geom1 减去 geom2)
        static std::string difference(const std::string& geom1_wkt, const std::string& geom2_wkt);

        /// @brief 缓冲区几何 WKT
        static std::string buffer(const std::string& geom_wkt, double distance);

        /// @brief 几何面积
        static double area(const std::string& geom_wkt);

        /// @brief 几何长度
        static double length(const std::string& geom_wkt);

        /// @brief 面几何周长
        static double perimeter(const std::string& geom_wkt);

        /// @brief 几何质心
        static Point centroid(const std::string& geom_wkt);

        /// @brief 按容差简化几何 (Douglas-Peucker)
        static std::string simplify(const std::string& geom_wkt, double tolerance);

        /// @brief 几何是否有效
        static bool isValid(const std::string& geom_wkt);

        /// @brief 修复无效几何; 已有效则原样返回
        static std::string makeValid(const std::string& geom_wkt);
    };

}  // namespace soilwatercm::infrastructure::gdal
