#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../infrastructure/external/odm_client.h"
#include "../infrastructure/gdal/gdal_wrapper.h"
#include "../models/common/geo_types.h"

namespace soilwatercm::services
{

    /** @brief 3D Tiles 转换入参 */
    struct ThreeDTilesParams
    {
        std::string input_path;          ///< 输入模型/栅格路径
        std::string output_path;         ///< 输出目录
        double max_error = 0.1;          ///< 最大误差 (米)
    };

    /** @brief 土石方计算入参 */
    struct EarthworkParams
    {
        std::string dem_before_path;            ///< 施工前 DEM 路径
        std::string dem_after_path;             ///< 施工后 DEM 路径
        std::optional<models::Geometry> area;   ///< 计算范围; 为空则全图
    };

    /** @brief 淹没分析入参 */
    struct InundationParams
    {
        std::string dem_path;                   ///< DEM 路径
        double water_level;                     ///< 水位高程
        std::optional<models::Geometry> area;   ///< 分析范围; 为空则全图
    };

    /** @brief 视域分析入参 */
    struct ViewshedParams
    {
        std::string dem_path;        ///< DEM 路径
        double observer_x;           ///< 观察点 x 坐标
        double observer_y;           ///< 观察点 y 坐标
        double observer_height;      ///< 观察点高程 (相对地面)
        double max_distance;         ///< 最大视距
    };

    /**
     * @brief 三维分析服务
     *
     * 模型/栅格转 3D Tiles (ODM 可用则走 ODM)、前后 DEM 差值算土石方、
     * 水位淹没范围与视线遮挡视域分析, 栅格结果写到输入文件同目录。
     */
    class ThreeDService
    {
    public:
        /**
         * @brief 构造, 注入 GDAL 封装与 ODM 客户端
         * @param gdal GDAL 封装 (读栅格)
         * @param odm ODM 客户端 (3D 模型转换)
         */
        explicit ThreeDService(std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal,
                               std::shared_ptr<infrastructure::external::OdmClient> odm);

        ~ThreeDService() = default;

        /**
         * @brief 转 3D Tiles
         *
         * obj/ply/osgb 优先走 ODM, ODM 不可用则手写简化 tileset.json;
         * tif 按栅格范围生成。
         * @param params 转换入参
         * @return 输出目录路径
         * @throw FileNotFoundException 输入文件不存在时
         */
        std::string convertTo3DTiles(const ThreeDTilesParams& params);

        /**
         * @brief 土石方计算
         *
         * 逐像元求 (后-前) 高差乘像元面积, 分别累计挖/填方量, 返回挖填差。
         * @param params 计算入参 (前后 DEM 尺寸须一致)
         * @return 挖填差 (立方米)
         * @throw ValidationException 前后 DEM 尺寸不一致时
         */
        double calculateEarthwork(const EarthworkParams& params);

        /**
         * @brief 淹没分析
         *
         * DEM 低于水位的像元判为淹没, 输出淹没掩膜与水深两张栅格
         * (输入路径 + _inundation.tif / _depth.tif)。
         * @param params 分析入参
         * @return 淹没掩膜路径
         */
        std::string analyzeInundation(const InundationParams& params);

        /**
         * @brief 视域分析
         *
         * 逐像元沿视线做剖面通视判断 (仰角比较), 输出 0/1 视域栅格。
         * @param params 分析入参
         * @return 视域栅格路径
         */
        std::string analyzeViewshed(const ViewshedParams& params);

    private:
        std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal_;
        std::shared_ptr<infrastructure::external::OdmClient> odm_;
    };

}  // namespace soilwatercm::services
