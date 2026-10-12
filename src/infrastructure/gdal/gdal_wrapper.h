#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <gdal.h>
#include <gdal_priv.h>
#include <ogr_spatialref.h>

namespace soilwatercm::infrastructure::gdal
{

    /// 栅格像元数据 (展平的 double 数组 + 仿射参数 + 投影)
    struct RasterData
    {
        std::vector<double> data;
        int width;
        int height;
        int bands;
        std::array<double, 6> geotransform;
        std::string projection;
        GDALDataType data_type;

        RasterData() : width(0), height(0), bands(1), geotransform{}, data_type(GDT_Float64) {}
    };

    /// 几何 (WKT 文本 + SRID)
    struct Geometry
    {
        std::string wkt;
        int srid = 4326;
        bool is_valid = false;

        Geometry() = default;
        Geometry(const std::string& w, int s = 4326) : wkt(w), srid(s), is_valid(!w.empty()) {}
    };

    /// 栅格元信息, 不含像元数据
    struct RasterInfo
    {
        int width = 0;
        int height = 0;
        int bandCount = 0;
        std::string crs;
        double resolution = 0.0;
        double xMin = 0.0, yMin = 0.0, xMax = 0.0, yMax = 0.0;
    };

    /**
     * @brief GDAL/OGR 底层封装
     *
     * 栅格读写、投影变换, 以及 WKT 几何的校验、量算与格式读取。
     */
    class GdalWrapper
    {
    public:
        /// @brief 进程启动时调用一次, 注册全部 GDAL 驱动
        static void init();

        /// @brief 进程退出时调用一次, 释放 GDAL 驱动管理器
        static void cleanup();

        GdalWrapper();
        ~GdalWrapper();

        /**
         * @brief 读取栅格全部波段为 double 数组
         * @param file_path 栅格文件路径
         * @return 像元数据; 打开失败返回空数据
         */
        RasterData readRaster(const std::string& file_path);

        /**
         * @brief 读取栅格指定波段
         * @param file_path 栅格文件路径
         * @param bandIndex 波段号, 从 1 开始
         * @return 像元数据
         */
        RasterData readRaster(const std::string& file_path, int bandIndex);

        /**
         * @brief 写出栅格文件
         * @param file_path 输出路径
         * @param data 像元数据
         * @param format GDAL 驱动名, 默认 GTiff
         * @return 成功返回 true; 驱动不存在抛异常
         */
        bool writeRaster(const std::string& file_path, const RasterData& data, const std::string& format = "GTiff");

        /// @brief 读文件投影 WKT, 失败返回空串
        std::string getProjection(const std::string& file_path);

        /// @brief 改写文件投影 (不做像元重采样)
        bool setProjection(const std::string& file_path, const std::string& projection);

        /// @brief 读仿射六参数, 失败返回空
        std::vector<double> getGeoTransform(const std::string& file_path);

        /// @brief 写仿射参数, transform 长度不是 6 返回 false
        bool setGeoTransform(const std::string& file_path, const std::vector<double>& transform);

        /**
         * @brief 重投影并写出新文件 (GDALWarpOperation 逐块搬运)
         * @param input_file 输入栅格
         * @param output_file 输出栅格
         * @param target_srs 目标投影 WKT
         * @return 输出路径; 失败抛异常
         */
        std::string reproject(const std::string& input_file, const std::string& output_file,
                              const std::string& target_srs);

        /// @brief 按 EPSG 码重投影
        std::string reproject(const std::string& input_file, const std::string& output_file, int target_epsg);

        /// @brief 读文件使用的驱动名
        std::string getDriverName(const std::string& file_path);

        /// @brief 读栅格元数据键值列表
        std::vector<std::string> getMetadata(const std::string& file_path);

        /// @brief 只读元信息 (尺寸/波段/CRS/分辨率/范围), 不加载像元数据
        RasterInfo getRasterInfo(const std::string& file_path);

        /// @brief WKT 能否解析且几何有效
        bool validateGeometry(const std::string& wkt);

        /// @brief 几何质心; 解析失败返回 nullopt
        std::optional<std::pair<double, double>> calculateCentroid(const std::string& wkt);

        /// @brief 几何面积 (面/多多面体); 非面几何返回 0
        double calculateArea(const std::string& wkt);

        /// @brief 几何长度 (线); 非线几何返回 0
        double calculateLength(const std::string& wkt);

        /**
         * @brief 几何在两个 EPSG 间坐标变换
         * @param wkt 源几何 WKT
         * @param sourceEpsg 源 EPSG
         * @param targetEpsg 目标 EPSG
         * @return 变换后 WKT; 失败返回空串
         */
        std::string transformGeometry(const std::string& wkt, int sourceEpsg, int targetEpsg);

        /// @brief 单 POLYGON 转 MULTIPOLYGON, 适配 PostGIS 入库要求
        std::string ensureMultiPolygon(const std::string& wkt);

        /// @brief 几何补 Z 维 (无 Z 时按 0 补), 适配 PostGIS 入库要求
        std::string ensure3D(const std::string& wkt);

        /// @brief 从 GeoJSON 读首个几何, 返回 WKT + SRID
        Geometry readGeometryFromGeoJson(const std::string& filePath);

        /// @brief 从 Shapefile 读首个几何
        Geometry readGeometryFromShapefile(const std::string& filePath);

        /// @brief 从 DXF 读首个几何
        Geometry readGeometryFromDxf(const std::string& filePath);

        /// @brief 从 KML 读首个几何
        Geometry readGeometryFromKml(const std::string& filePath);

        GdalWrapper(const GdalWrapper&) = delete;
        GdalWrapper& operator=(const GdalWrapper&) = delete;
        GdalWrapper(GdalWrapper&&) = default;
        GdalWrapper& operator=(GdalWrapper&&) = default;

    private:
        static std::string getLastError();
        static OGRGeometry* parseWkt(const std::string& wkt);
        static std::string geometryToWkt(OGRGeometry* geom);
    };

}  // namespace soilwatercm::infrastructure::gdal
