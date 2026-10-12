#pragma once

#include <drogon/HttpController.h>

#include "../services/scene_layer_service.h"

namespace soilwatercm::controllers
{

    /**
     * @brief 三维场景图层 HTTP 控制器
     *
     * 全部接口返回 GeoJSON FeatureCollection, 供 Cesium 渲染。
     */
    class SceneLayerController : public drogon::HttpController<SceneLayerController, false>
    {
    public:
        SceneLayerController() = default;

        /**
         * @brief 构造, 注入场景图层服务
         * @param sceneLayerService 三维场景图层服务
         */
        explicit SceneLayerController(std::shared_ptr<services::SceneLayerService> sceneLayerService);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(SceneLayerController::disturbances, "/api/v1/scene-layers/disturbances", drogon::Get);
        ADD_METHOD_TO(SceneLayerController::measures, "/api/v1/scene-layers/measures", drogon::Get);
        ADD_METHOD_TO(SceneLayerController::spoilDumps, "/api/v1/scene-layers/spoil-dumps", drogon::Get);
        ADD_METHOD_TO(SceneLayerController::anomalies, "/api/v1/scene-layers/anomalies", drogon::Get);
        ADD_METHOD_TO(SceneLayerController::insarResults, "/api/v1/scene-layers/insar-results", drogon::Get);
        ADD_METHOD_TO(SceneLayerController::inundationGeoJson, "/api/v1/scene-layers/inundation-geojson", drogon::Get);
        ADD_METHOD_TO(SceneLayerController::viewshedGeoJson, "/api/v1/scene-layers/viewshed-geojson", drogon::Get);
        ADD_METHOD_TO(SceneLayerController::availability, "/api/v1/scene-layers/availability", drogon::Get);
        METHOD_LIST_END

        /**
         * @brief GET /api/v1/scene-layers/disturbances?project_id=&period_id= 扰动图斑图层
         * @return 通过 callback 返回 FeatureCollection
         */
        void disturbances(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/scene-layers/measures?project_id=&period_id=&category= 水保措施图层
         * @return 通过 callback 返回 FeatureCollection; category 为空则不过滤
         */
        void measures(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/scene-layers/spoil-dumps?project_id=&period_id= 弃渣场图层
         * @return 通过 callback 返回 FeatureCollection; 底层表缺失, 当前恒为空集合
         */
        void spoilDumps(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/scene-layers/anomalies?project_id=&period_id= 异常点位图层
         * @return 通过 callback 返回 FeatureCollection
         */
        void anomalies(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/scene-layers/insar-results?project_id= InSAR 形变成果图层
         * @return 通过 callback 返回 FeatureCollection; 底层表缺失, 当前恒为空集合
         */
        void insarResults(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/scene-layers/inundation-geojson?project_id=&result_id= 淹没分析成果图层
         * @return 通过 callback 返回 FeatureCollection
         */
        void inundationGeoJson(const drogon::HttpRequestPtr& req,
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/scene-layers/viewshed-geojson?project_id=&result_id= 通视分析成果图层
         * @return 通过 callback 返回 FeatureCollection
         */
        void viewshedGeoJson(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/scene-layers/availability?project_id=&period_id= 各图层数据可用性统计
         * @return 通过 callback 返回可用性统计
         */
        void availability(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    private:
        std::shared_ptr<services::SceneLayerService> sceneLayerService_;
    };

}  // namespace soilwatercm::controllers
