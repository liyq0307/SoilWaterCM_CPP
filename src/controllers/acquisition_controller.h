#pragma once

#include <drogon/HttpController.h>

#include "../services/acquisition_service.h"

namespace soilwatercm::controllers
{

    /**
     * @brief 数据获取 HTTP 控制器
     *
     * 提供卫星影像检索/采集与无人机航线规划, 路由前缀 /api/v1/acquisition。
     */
    class AcquisitionController : public drogon::HttpController<AcquisitionController, false>
    {
    public:
        AcquisitionController() = default;

        /**
         * @brief 构造, 注入数据获取服务
         * @param acquisitionService 数据获取服务
         */
        explicit AcquisitionController(std::shared_ptr<services::AcquisitionService> acquisitionService);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(AcquisitionController::searchSatellite, "/api/v1/acquisition/satellite/search", drogon::Post);
        ADD_METHOD_TO(AcquisitionController::collectSatellite, "/api/v1/acquisition/satellite/collect", drogon::Post);
        ADD_METHOD_TO(AcquisitionController::flightPlan, "/api/v1/acquisition/flight-plan", drogon::Post);
        ADD_METHOD_TO(AcquisitionController::backendStatus, "/api/v1/acquisition/backend-status", drogon::Get);
        METHOD_LIST_END

        /**
         * @brief POST /api/v1/acquisition/satellite/search 同步检索可用卫星景
         * @return 通过 callback 返回检索结果 (sentinel/landsat/china)
         */
        void searchSatellite(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/acquisition/satellite/collect 异步采集卫星数据
         *
         * 创建任务后台检索下载。
         * @return 通过 callback 返回 201 与任务信息
         */
        void collectSatellite(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/acquisition/flight-plan 无人机航线规划
         *
         * 按 bbox、GSD 与相机参数计算航高、航带与航点。
         * @return 通过 callback 返回航线方案
         */
        void flightPlan(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/acquisition/backend-status 数据源可用性诊断
         * @return 通过 callback 返回各卫星数据源状态
         */
        void backendStatus(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    private:
        std::shared_ptr<services::AcquisitionService> acquisitionService_;
    };

}  // namespace soilwatercm::controllers
