#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "../dao/anomaly_dao.h"
#include "../dao/disturbance_dao.h"
#include "../dao/erosion_result_dao.h"
#include "../dao/measure_dao.h"
#include "../dao/monitoring_period_dao.h"
#include "../dao/spoil_dump_dao.h"
#include "../dao/terrain_result_dao.h"
#include "../dao/vegetation_coverage_dao.h"
#include "../infrastructure/gdal/vector_processor.h"

namespace soilwatercm::services
{

    /**
     * @brief 三维场景图层服务
     *
     * 输出 GeoJSON FeatureCollection 供 Cesium 渲染
     * (移植自 Python 后端 app/api/v1/endpoints/scene_layers.py)。
     */
    class SceneLayerService
    {
    public:
        /**
         * @brief 构造, 注入各成果 DAO 与矢量化工具
         * @param disturbanceDao 扰动图斑 DAO
         * @param measureDao 水保措施 DAO
         * @param anomalyDao 异常记录 DAO
         * @param periodDao 监测期 DAO (期次归属校验与聚合查询)
         * @param fvcDao 植被覆盖度 DAO
         * @param erosionDao 侵蚀结果 DAO
         * @param terrainDao 地形/淹没/视域/InSAR 成果 DAO
         * @param spoilDao 弃渣场 DAO
         * @param vectorProcessor 栅格矢量化 (淹没/视域 GeoJSON)
         */
        SceneLayerService(std::shared_ptr<dao::DisturbanceDao> disturbanceDao,
                          std::shared_ptr<dao::MeasureDao> measureDao, std::shared_ptr<dao::AnomalyDao> anomalyDao,
                          std::shared_ptr<dao::MonitoringPeriodDao> periodDao,
                          std::shared_ptr<dao::VegetationCoverageDao> fvcDao,
                          std::shared_ptr<dao::ErosionResultDao> erosionDao,
                          std::shared_ptr<dao::TerrainResultDao> terrainDao = nullptr,
                          std::shared_ptr<dao::SpoilDumpDao> spoilDao = nullptr,
                          std::shared_ptr<infrastructure::gdal::VectorProcessor> vectorProcessor = nullptr);

        // ---- 有真实数据的图层 ----

        /**
         * @brief 扰动图斑图层
         * @param projectId 项目 id
         * @param periodId 期次 id; 为空则全期次
         * @return FeatureCollection
         */
        nlohmann::json disturbances(int projectId, std::optional<int> periodId);

        /**
         * @brief 水保措施图层
         * @param projectId 项目 id
         * @param periodId 期次 id; 为空则全期次
         * @param category 措施类别; 为空则不过滤
         * @return FeatureCollection
         */
        nlohmann::json measures(int projectId, std::optional<int> periodId, const std::string& category);

        /**
         * @brief 异常事件图层
         * @param projectId 项目 id
         * @param periodId 期次 id; 为空则全期次
         * @return FeatureCollection
         */
        nlohmann::json anomalies(int projectId, std::optional<int> periodId);

        // ---- 缺少底层表, 暂返回空 FeatureCollection ----

        /**
         * @brief 弃渣场图层; 缺少底层表, 暂返回空 FeatureCollection
         * @param projectId 项目 id
         * @param periodId 期次 id; 为空则全期次
         * @return FeatureCollection
         */
        nlohmann::json spoilDumps(int projectId, std::optional<int> periodId);

        /**
         * @brief InSAR 形变成果列表 (terrain_results analysis=insar)
         * @param projectId 项目 id
         * @return 成果 JSON 列表 (id/raster_path/stats/created_at)
         */
        nlohmann::json insarResults(int projectId);

        /**
         * @brief 淹没范围图层; 最新 terrain_results (analysis=inundation) 栅格矢量化
         * @param projectId 项目 id
         * @param resultId 结果 id; 可空
         * @return FeatureCollection
         */
        nlohmann::json inundationGeoJson(int projectId, std::optional<int> resultId);

        /**
         * @brief 视域范围图层; 最新 terrain_results (analysis=viewshed) 栅格矢量化
         * @param projectId 项目 id
         * @param resultId 结果 id; 可空
         * @return FeatureCollection
         */
        nlohmann::json viewshedGeoJson(int projectId, std::optional<int> resultId);

        // ---- 图层数据可用性 ----

        /**
         * @brief 图层数据可用性 (矢量计数 + 栅格存在标志; 缺失维度补 0/false)
         * @param projectId 项目 id
         * @param periodId 期次 id; 为空则全期次
         * @return 各图层可用性 JSON
         */
        nlohmann::json availability(int projectId, std::optional<int> periodId);

    private:
        // 项目下全部监测期 id (用于无 period_id 过滤时聚合查询)
        std::vector<int> periodIdsForProject(int projectId);
        // 校验 period 属于该 project; periodId 为空视为通过
        bool periodBelongsToProject(int projectId, std::optional<int> periodId);

        std::shared_ptr<dao::DisturbanceDao> disturbanceDao_;
        std::shared_ptr<dao::MeasureDao> measureDao_;
        std::shared_ptr<dao::AnomalyDao> anomalyDao_;
        std::shared_ptr<dao::MonitoringPeriodDao> periodDao_;
        std::shared_ptr<dao::VegetationCoverageDao> fvcDao_;
        std::shared_ptr<dao::ErosionResultDao> erosionDao_;
        std::shared_ptr<dao::TerrainResultDao> terrainDao_;
        std::shared_ptr<dao::SpoilDumpDao> spoilDao_;
        std::shared_ptr<infrastructure::gdal::VectorProcessor> vectorProcessor_;
    };

}  // namespace soilwatercm::services
