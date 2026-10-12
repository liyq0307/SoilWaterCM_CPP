#pragma once

#include <drogon/HttpController.h>

#include "../services/indicator_service.h"

namespace soilwatercm::controllers
{

    /**
     * @brief 监测指标 HTTP 控制器
     *
     * 提供指标定义维护与指标值查询, 路由前缀 /api/v1/indicators。
     */
    class IndicatorController : public drogon::HttpController<IndicatorController, false>
    {
    public:
        IndicatorController() = default;

        /**
         * @brief 构造, 注入指标服务
         * @param indicatorService 监测指标服务
         */
        explicit IndicatorController(std::shared_ptr<services::IndicatorService> indicatorService);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(IndicatorController::listIndicators, "/api/v1/indicators", drogon::Get);
        ADD_METHOD_TO(IndicatorController::updateWeight, "/api/v1/indicators/{id}/weight", drogon::Put);
        ADD_METHOD_TO(IndicatorController::updateThresholds, "/api/v1/indicators/{id}/thresholds", drogon::Put);
        ADD_METHOD_TO(IndicatorController::listIndicatorValues, "/api/v1/indicators/values", drogon::Get);
        METHOD_LIST_END

        /**
         * @brief GET /api/v1/indicators?dimension= 列出指标定义
         * @return 通过 callback 返回指标列表; dimension 为空则返回全部
         */
        void listIndicators(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief PUT /api/v1/indicators/{id}/weight 设置指标权重
         * @param id 指标 id
         * @return 通过 callback 返回更新后的权重
         */
        void updateWeight(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief PUT /api/v1/indicators/{id}/thresholds 设置 good/warn/bad 分级阈值
         * @param id 指标 id
         * @return 通过 callback 返回更新后的阈值
         */
        void updateThresholds(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief GET /api/v1/indicators/values?project_id=&period_id=&indicator_id= 查询指标计算值
         * @return 通过 callback 返回指标值列表
         */
        void listIndicatorValues(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    private:
        std::shared_ptr<services::IndicatorService> indicatorService_;
    };

}  // namespace soilwatercm::controllers
