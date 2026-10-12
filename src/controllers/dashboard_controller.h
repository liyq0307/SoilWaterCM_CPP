#pragma once

#include <drogon/HttpController.h>

#include "../services/dashboard_service.h"

namespace soilwatercm::controllers
{

    /**
     * @brief 数据看板 HTTP 控制器
     *
     * 提供全局统计、工作台与项目级汇总/趋势, 路由前缀 /api/v1/dashboard。
     */
    class DashboardController : public drogon::HttpController<DashboardController, false>
    {
    public:
        DashboardController() = default;

        /**
         * @brief 构造, 注入看板服务
         * @param dashboardService 数据看板服务
         */
        explicit DashboardController(std::shared_ptr<services::DashboardService> dashboardService);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(DashboardController::getDashboard, "/api/v1/dashboard", drogon::Get);
        ADD_METHOD_TO(DashboardController::getWorkbench, "/api/v1/dashboard/workbench", drogon::Get);
        ADD_METHOD_TO(DashboardController::getSummary, "/api/v1/dashboard/summary", drogon::Get);
        ADD_METHOD_TO(DashboardController::getTrend, "/api/v1/dashboard/trend", drogon::Get);
        ADD_METHOD_TO(DashboardController::getMultiTrend, "/api/v1/dashboard/multi-trend", drogon::Get);
        ADD_METHOD_TO(DashboardController::getPeriodIndicators, "/api/v1/dashboard/period-indicators", drogon::Get);
        METHOD_LIST_END

        /**
         * @brief GET /api/v1/dashboard 全局统计
         * @return 通过 callback 返回项目/影像/任务数量与异常待处理数
         */
        void getDashboard(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/dashboard/workbench?user_id= 工作台数据
         * @return 通过 callback 返回最近项目、任务、待处理异常与快捷统计
         */
        void getWorkbench(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/dashboard/summary?project_id= 单项目监测汇总
         * @return 通过 callback 返回汇总数据
         */
        void getSummary(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/dashboard/trend?project_id=&indicator_code= 单指标跨期趋势
         * @return 通过 callback 返回趋势序列
         */
        void getTrend(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/dashboard/multi-trend?project_id= 全指标跨期趋势
         * @return 通过 callback 返回趋势序列集合
         */
        void getMultiTrend(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/dashboard/period-indicators?project_id=&period_id= 指定监测期的指标值
         * @return 通过 callback 返回指标值列表
         */
        void getPeriodIndicators(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    private:
        std::shared_ptr<services::DashboardService> dashboardService_;
    };

}  // namespace soilwatercm::controllers
