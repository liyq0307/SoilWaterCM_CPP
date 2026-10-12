#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../dao/monitoring_period_dao.h"
#include "../dao/project_dao.h"
#include "../dao/project_document_dao.h"
#include "../infrastructure/gdal/gdal_wrapper.h"
#include "../models/monitoring_period.h"
#include "../models/project.h"
#include "../models/project_document.h"

namespace soilwatercm::services
{

    /** @brief 新建项目入参 */
    struct ProjectCreateParams
    {
        std::string code;                              ///< 项目编码 (唯一)
        std::string name;                              ///< 项目名称
        std::optional<models::Geometry> boundary;      ///< 责任边界几何 (可空)
        std::optional<double> center_lon;              ///< 中心点经度 (可空, 带边界时可自动算)
        std::optional<double> center_lat;              ///< 中心点纬度
        models::ProjectStatus status = models::ProjectStatus::Active;  ///< 初始状态
    };

    /** @brief 更新项目入参 (字段为空则不改) */
    struct ProjectUpdateParams
    {
        std::optional<std::string> name;               ///< 新名称
        std::optional<models::Geometry> boundary;      ///< 新责任边界
        std::optional<double> center_lon;              ///< 新中心点经度
        std::optional<double> center_lat;              ///< 新中心点纬度
        std::optional<models::ProjectStatus> status;   ///< 新状态
    };

    /** @brief 项目列表过滤条件 */
    struct ProjectListFilter
    {
        std::optional<models::ProjectStatus> status;   ///< 按状态过滤
        std::optional<std::string> keyword;            ///< 名称/编码关键字过滤
        int page = 1;                                  ///< 页码, 从 1 开始
        int pageSize = 20;                             ///< 每页条数
    };

    /**
     * @brief 项目与责任边界管理服务
     *
     * 负责项目 CRUD、边界文件上传解析 (GeoJSON/SHP/zip/DXF/KML, 统一转 WGS84
     * MultiPolygon 3D)、项目资料文件管理, 以及几何校验/质心/面积计算。
     */
    class ProjectService
    {
    public:
        /**
         * @brief 构造, 注入项目/监测期 DAO、GDAL 与资料 DAO
         * @param projectDao 项目 DAO
         * @param periodDao 监测期 DAO (删除项目前检查)
         * @param gdal GDAL 封装 (几何校验/转换)
         * @param documentDao 项目资料 DAO; 为空则首次使用时按全局配置惰性创建
         */
        explicit ProjectService(std::shared_ptr<dao::ProjectDao> projectDao,
                                std::shared_ptr<dao::MonitoringPeriodDao> periodDao,
                                std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal,
                                std::shared_ptr<dao::ProjectDocumentDao> documentDao = nullptr);

        ~ProjectService() = default;

        /**
         * @brief 创建项目
         *
         * 带边界时自动校验几何并算质心作中心点。
         * @param params 新建入参
         * @return 含新 id 的项目记录
         * @throw ValidationException 编码重复或边界几何无效时
         */
        models::Project createProject(const ProjectCreateParams& params);

        /**
         * @brief 按 id 查项目
         * @param id 项目主键
         * @return 项目; 不存在返回 nullopt
         */
        std::optional<models::Project> getProjectById(int id);

        /**
         * @brief 按编码查项目
         * @param code 项目编码
         * @return 项目; 不存在返回 nullopt
         */
        std::optional<models::Project> getProjectByCode(const std::string& code);

        /**
         * @brief 分页列项目
         *
         * 状态/关键字过滤在内存中做 (先全量取出再筛)。
         * @param filter 过滤与分页条件
         * @return 当前页项目列表
         */
        std::vector<models::Project> listProjects(const ProjectListFilter& filter = {});

        /**
         * @brief 更新项目
         *
         * 换边界且未指定中心点时按新边界重算质心。
         * @param id 项目主键
         * @param params 更新入参 (字段为空则不改)
         * @return 更新成功返回 true
         * @throw NotFoundException 项目不存在时
         * @throw ValidationException 新边界几何无效时
         */
        bool updateProject(int id, const ProjectUpdateParams& params);

        /**
         * @brief 删除项目
         * @param id 项目主键
         * @return 删除成功返回 true
         * @throw NotFoundException 项目不存在时
         * @throw ValidationException 项目下存在监测期时
         */
        bool deleteProject(int id);

        /**
         * @brief 上传边界文件并更新责任边界
         * @param projectId 项目 id
         * @param filePath 已落盘的文件路径
         * @param format 文件格式 (决定 GDAL 驱动)
         * @param sourceEpsg 源坐标系 EPSG; 为空则按文件自带 CRS
         * @return 更新成功返回 true
         * @throw NotFoundException 项目不存在时
         * @throw ValidationException 不支持的格式或几何无效时
         */
        bool uploadBoundary(int projectId, const std::string& filePath, const std::string& format,
                            std::optional<int> sourceEpsg = std::nullopt);

        /**
         * @brief 读取项目责任边界几何
         * @param projectId 项目 id
         * @return 边界几何; 项目或边界不存在返回 nullopt
         */
        std::optional<models::Geometry> getBoundary(int projectId);

        /**
         * @brief 上传内存中的边界文件内容 (multipart)
         *
         * 落盘临时文件 -> GDAL 读取 -> 更新 responsibility_area -> 删临时文件。
         * 格式由文件扩展名推断 (geojson/json/shp/zip/dxf/kml/kmz)。
         * @param projectId 项目 id
         * @param fileName 原始文件名 (取扩展名)
         * @param content 文件内容
         * @param sourceEpsg 源坐标系 EPSG; 为空则按文件自带 CRS
         * @return 边界面积 (公顷)
         * @throw NotFoundException 项目不存在时
         * @throw ValidationException 不支持的格式或几何无效时
         */
        double uploadBoundaryContent(int projectId, const std::string& fileName, const std::string& content,
                                     std::optional<int> sourceEpsg = std::nullopt);

        /**
         * @brief 从已落盘路径读取边界并更新
         * @param projectId 项目 id
         * @param filePath 路径, 可为 .geojson/.shp/.zip/.dxf/.kml/.kmz (.shp 需同目录配套文件)
         * @param sourceEpsg 源坐标系 EPSG; 为空则按文件自带 CRS
         * @return 边界面积 (公顷)
         * @throw NotFoundException 项目不存在时
         * @throw ValidationException 不支持的格式 / 未找到有效多边形 / 几何无效时
         */
        double uploadBoundaryFromPath(int projectId, const std::string& filePath,
                                      std::optional<int> sourceEpsg = std::nullopt);

        // ---- 项目资料 (project_documents) ----

        /**
         * @brief 列出项目资料
         * @param projectId 项目 id
         * @return 资料记录列表
         * @throw NotFoundException 项目不存在时
         */
        std::vector<models::ProjectDocument> listDocuments(int projectId);

        /**
         * @brief 上传资料文件: 落盘到 {raw}/project_{id}/docs/ 并入库
         * @param projectId 项目 id
         * @param docType 资料类型
         * @param title 资料标题; 为空则用文件名
         * @param fileName 原始文件名
         * @param content 文件内容
         * @return 含新 id 的资料记录
         * @throw NotFoundException 项目不存在时
         * @throw ValidationException 文件名为空时
         * @throw DatabaseException 资料入库失败时
         */
        models::ProjectDocument uploadDocument(int projectId, const std::string& docType,
                                                const std::optional<std::string>& title, const std::string& fileName,
                                                const std::string& content);

        /**
         * @brief 取资料记录
         * @param projectId 项目 id (资料不属于该项目视为不存在)
         * @param docId 资料主键
         * @return 资料记录; 不存在返回 nullopt
         */
        std::optional<models::ProjectDocument> getDocument(int projectId, int docId);

        /**
         * @brief 删除资料 (删记录 + 物理文件)
         * @param projectId 项目 id
         * @param docId 资料主键
         * @return 删除成功返回 true
         * @throw NotFoundException 资料不存在或不属于该项目时
         */
        bool deleteDocument(int projectId, int docId);

        /**
         * @brief 列出项目监测期
         * @param projectId 项目 id
         * @return 监测期列表
         */
        std::vector<models::MonitoringPeriod> getMonitoringPeriods(int projectId);

        /**
         * @brief 统计项目监测期数量
         * @param projectId 项目 id
         * @return 期次数
         */
        int getMonitoringPeriodCount(int projectId);

        /**
         * @brief GDAL 几何校验
         * @param geometry 待校验几何 (空 WKT 直接 false)
         * @return 有效返回 true
         */
        bool validateGeometry(const models::Geometry& geometry);

        /**
         * @brief 计算几何质心
         * @param geometry 几何
         * @return (经度, 纬度); 失败返回 nullopt
         */
        std::optional<std::pair<double, double>> calculateCentroid(const models::Geometry& geometry);

        /**
         * @brief 计算几何面积 (GDAL 平面面积)
         * @param geometry 几何
         * @return 面积; 失败返回负值
         */
        double calculateArea(const models::Geometry& geometry);

    private:
        std::shared_ptr<dao::ProjectDao> projectDao_;
        std::shared_ptr<dao::MonitoringPeriodDao> periodDao_;
        std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal_;
        std::shared_ptr<dao::ProjectDocumentDao> documentDao_;

        /** @brief 资料 DAO, 未注入时按全局配置惰性创建 */
        std::shared_ptr<dao::ProjectDocumentDao> documentDao();

        models::Geometry transformToWgs84(const models::Geometry& geometry, int sourceEpsg);
        models::Geometry ensureMultiPolygon(const models::Geometry& geometry);
        models::Geometry ensure3D(const models::Geometry& geometry);
    };

}  // namespace soilwatercm::services
