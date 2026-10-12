#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../infrastructure/external/qgis_client.h"
#include "../infrastructure/gdal/gdal_wrapper.h"

namespace soilwatercm::services
{

    /** @brief 图层发布入参 */
    struct PublishLayerParams
    {
        std::string layer_name;                 ///< 图层名
        std::string data_path;                  ///< 数据路径
        std::string style_name = "default";     ///< 样式名
        std::optional<std::string> title;       ///< 图层标题
        std::optional<std::string> abstract;    ///< 图层摘要
    };

    /** @brief QGIS 工程信息 */
    struct QgsProjectInfo
    {
        std::string name;                   ///< 工程名称
        std::string path;                   ///< 工程文件路径
        std::vector<std::string> layers;    ///< 图层列表
        std::string crs;                    ///< 坐标参考系
    };

    /** @brief WMS 服务定位信息 */
    struct QgisWmsInfo
    {
        std::string qgsPath;             ///< 工程路径
        std::string service;             ///< 服务名
        bool fallback = false;           ///< 是否回退命中 (非精确工程名)
        std::string wmsBase;             ///< WMS 服务地址
        std::string getCapabilitiesUrl;  ///< GetCapabilities 地址
    };

    /**
     * @brief QGIS 服务发布
     *
     * 生成 .qgs 工程与 .qml 样式文件, 通过 QGIS Server 客户端发布/下架图层服务,
     * 并按项目/期次定位 WMS 地址 (精确名未命中时回退最新工程)。
     */
    class QgisService
    {
    public:
        /**
         * @brief 构造, 注入 QGIS Server 客户端与 GDAL 封装
         * @param qgisClient QGIS Server 客户端
         * @param gdal GDAL 封装 (读数据范围/CRS)
         */
        explicit QgisService(std::shared_ptr<infrastructure::external::QgisClient> qgisClient,
                             std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal);

        ~QgisService() = default;

        /**
         * @brief 发布图层
         * @param params 发布入参
         * @return 生成工程+样式文件后交给 QGIS Server, 成功返回 true
         */
        bool publishLayer(const PublishLayerParams& params);

        /**
         * @brief 手写生成 .qgs 工程文件
         * @param projectName 工程名
         * @param layerPaths 各数据路径 (列为图层, EPSG:4326)
         * @param outputPath 输出路径
         * @return 输出路径
         */
        std::string generateQgsProject(const std::string& projectName, const std::vector<std::string>& layerPaths,
                                       const std::string& outputPath);

        /**
         * @brief 生成 .qml 样式文件
         * @param layerName 图层名 (当前未使用)
         * @param styleType 样式类型 (当前未使用)
         * @param outputPath 输出路径
         * @return 输出路径; 当前为占位实现, 固定输出单一灰色填充样式
         */
        std::string generateQmlStyle(const std::string& layerName, const std::string& styleType,
                                     const std::string& outputPath);

        /**
         * @brief 扫描工程目录列出已发布服务
         * @return 工程信息列表 (目录不存在返回空列表)
         */
        std::vector<QgsProjectInfo> listServices();

        /**
         * @brief 删除服务工程文件
         * @param serviceName 服务名
         * @return 删除成功返回 true; 工程不存在返回 false
         */
        bool deleteService(const std::string& serviceName);

        /**
         * @brief 查询单个服务信息
         * @param serviceName 服务名
         * @return 工程信息; 不存在返回 nullopt
         */
        std::optional<QgsProjectInfo> getServiceInfo(const std::string& serviceName);

        /**
         * @brief 下架项目服务
         * @param projectId 项目 id
         * @param periodId 期次 id
         * @return 删除 project_{id}[_p{period}] 的工程/备份/样式文件并通知 Server,
         *         成功返回 true
         */
        bool unpublishProject(int projectId, int periodId);

        /**
         * @brief 定位项目 WMS 服务
         * @param projectId 项目 id
         * @param periodId 期次 id
         * @return WMS 定位信息; 优先精确匹配工程名, 未命中回退该项目最新发布工程,
         *         都没有返回 nullopt
         */
        std::optional<QgisWmsInfo> findWmsInfo(int projectId, int periodId);

    private:
        std::shared_ptr<infrastructure::external::QgisClient> qgisClient_;
        std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal_;
    };

}  // namespace soilwatercm::services
