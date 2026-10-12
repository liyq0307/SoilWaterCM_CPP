#pragma once

#include <drogon/HttpController.h>

#include "../infrastructure/gdal/vector_processor.h"
#include "../services/zone_service.h"

namespace soilwatercm::controllers
{

    /**
     * @brief 水保分区 HTTP 控制器
     *
     * 提供分区统计 CRUD、边界上传与分区归属分配, 路由前缀 /api/v1/zone-*。
     */
    class ZoneController : public drogon::HttpController<ZoneController, false>
    {
    public:
        ZoneController() = default;

        /**
         * @brief 构造, 注入分区服务与矢量处理器
         * @param zoneService 水保分区服务
         * @param vectorProcessor 矢量文件处理器 (边界上传解析)
         */
        explicit ZoneController(std::shared_ptr<services::ZoneService> zoneService,
                                std::shared_ptr<infrastructure::gdal::VectorProcessor> vectorProcessor);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(ZoneController::listZoneStats, "/api/v1/zone-stats", drogon::Get);
        ADD_METHOD_TO(ZoneController::createZoneStats, "/api/v1/zone-stats", drogon::Post);
        ADD_METHOD_TO(ZoneController::updateZoneStats, "/api/v1/zone-stats/{id}", drogon::Put);
        ADD_METHOD_TO(ZoneController::deleteZoneStats, "/api/v1/zone-stats/{id}", drogon::Delete);
        ADD_METHOD_TO(ZoneController::listZoneCodes, "/api/v1/zone-codes", drogon::Get);
        ADD_METHOD_TO(ZoneController::listZoneBoundaries, "/api/v1/zone-boundaries", drogon::Get);
        ADD_METHOD_TO(ZoneController::uploadZoneBoundaries, "/api/v1/zone-boundaries", drogon::Post);
        ADD_METHOD_TO(ZoneController::uploadZoneBoundaryFile, "/api/v1/zone-boundaries/upload", drogon::Post);
        ADD_METHOD_TO(ZoneController::initZoneStats, "/api/v1/init-zone-stats", drogon::Post);
        ADD_METHOD_TO(ZoneController::assignZones, "/api/v1/assign-zones", drogon::Post);
        METHOD_LIST_END

        /**
         * @brief GET /api/v1/zone-stats?project_id=&period_id=&zone= 列出分区统计
         * @return 通过 callback 返回统计列表; period_id/zone 为空则不过滤
         */
        void listZoneStats(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/zone-stats 创建分区统计记录
         * @return 通过 callback 返回 201 与新记录
         */
        void createZoneStats(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief PUT /api/v1/zone-stats/{id} 增量更新分区统计
         * @param id 分区统计 id
         * @return 通过 callback 返回更新后的记录; 仅更新 body 中出现的字段
         */
        void updateZoneStats(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief DELETE /api/v1/zone-stats/{id} 删除分区统计
         * @param id 分区统计 id
         * @return 通过 callback 返回 204
         */
        void deleteZoneStats(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief GET /api/v1/zone-codes 分区代码字典
         * @return 通过 callback 返回分区代码与中文名映射表
         */
        void listZoneCodes(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/zone-boundaries?project_id= 列出项目分区边界摘要
         * @return 通过 callback 返回边界摘要 (分区/名称/面积)
         */
        void listZoneBoundaries(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/zone-boundaries?project_id= 上传分区边界 GeoJSON
         *
         * 覆盖式写入, 并自动完成归属分配与统计初始化。
         * @return 通过 callback 返回写入结果
         */
        void uploadZoneBoundaries(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/zone-boundaries/upload?project_id= 上传边界矢量文件
         *
         * 支持 shp/kml/kmz/dxf/zip, 解析后写入分区边界。
         * @return 通过 callback 返回写入结果
         */
        void uploadZoneBoundaryFile(const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/init-zone-stats?project_id=&period_id= 初始化分区设计指标
         * @return 通过 callback 返回初始化结果; 补全各分区缺失的设计指标
         */
        void initZoneStats(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/assign-zones?project_id=&period_id= 空间套合分配分区归属
         * @return 通过 callback 返回分配结果; 回填扰动图斑与水保措施的分区归属
         */
        void assignZones(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    private:
        std::shared_ptr<services::ZoneService> zoneService_;
        std::shared_ptr<infrastructure::gdal::VectorProcessor> vectorProcessor_;
    };

}  // namespace soilwatercm::controllers
