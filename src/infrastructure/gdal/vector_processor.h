#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ogrsf_frmts.h>

#include "../../models/common/geo_types.h"
#include "gdal_wrapper.h"

namespace soilwatercm::infrastructure::gdal
{

    /// 矢量要素 (几何 WKT + 属性表)
    struct VectorFeature
    {
        std::string geometry;
        std::map<std::string, std::string> attributes;
    };

    /// 矢量数据集 (要素列表 + 投影 + 几何类型)
    struct VectorData
    {
        std::vector<VectorFeature> features;
        std::string projection;
        std::string geometry_type;
    };

    /**
     * @brief 矢量数据处理
     *
     * 读写 Shapefile 等格式, 几何叠加分析与栅格转矢量。
     */
    class VectorProcessor
    {
    public:
        VectorProcessor();
        ~VectorProcessor() = default;

        /**
         * @brief 读矢量文件 (取首图层全部要素)
         * @param file_path 文件路径
         * @return 矢量数据; 打开失败返回空
         */
        VectorData read(const std::string& file_path);

        /**
         * @brief 写矢量文件
         * @param file_path 输出路径
         * @param data 矢量数据
         * @param format OGR 驱动名, 默认 ESRI Shapefile
         * @return 成功返回 true
         */
        bool write(const std::string& file_path, const VectorData& data, const std::string& format = "ESRI Shapefile");

        /// @brief 全要素缓冲区
        VectorData buffer(const VectorData& input, double distance);

        /// @brief 两数据集逐要素求交
        VectorData intersect(const VectorData& input1, const VectorData& input2);

        /// @brief 两数据集几何合并
        VectorData union_(const VectorData& input1, const VectorData& input2);

        /// @brief 差集 (input1 减去 input2)
        VectorData difference(const VectorData& input1, const VectorData& input2);

        /// @brief 要素面积
        double calculateArea(const VectorFeature& feature);

        /// @brief 要素长度
        double calculateLength(const VectorFeature& feature);

        /// @brief 两要素间最短距离
        double calculateDistance(const VectorFeature& feature1, const VectorFeature& feature2);

        /// @brief 整个数据集重投影到目标 SRS
        VectorData reproject(const VectorData& input, const std::string& target_srs);

        /// @brief 按阈值二值化栅格后提取多边形 (GDALPolygonize)
        VectorData fromRaster(const RasterData& raster, double threshold);

        /// @brief 按属性值过滤要素 (字符串相等比较)
        std::vector<VectorFeature> filter(const VectorData& input, const std::string& attribute,
                                          const std::string& value);

        /// @brief 整幅栅格转矢量面 (首波段二值化后多边形化)
        std::vector<soilwatercm::models::Geometry> rasterToVector(const std::string& rasterPath);

        /// @brief 从影像中检测地物特征轮廓 (首波段直接多边形化)
        std::vector<soilwatercm::models::Geometry> detectFeatures(const std::string& imagePath);
    };

}  // namespace soilwatercm::infrastructure::gdal
