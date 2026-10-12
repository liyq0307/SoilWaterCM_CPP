#pragma once

#include <drogon/HttpController.h>

#include "../services/monitoring_service.h"

namespace soilwatercm::controllers
{

    /**
     * @brief 特殊时期监测 HTTP 控制器
     *
     * 提供调度建议与快速变化识别, 路由前缀 /api/v1/monitoring。
     */
    class MonitoringController : public drogon::HttpController<MonitoringController, false>
    {
    public:
        MonitoringController() = default;

        /**
         * @brief 构造, 注入特殊时期监测服务
         * @param monitoringService 特殊时期监测服务
         */
        explicit MonitoringController(std::shared_ptr<services::MonitoringService> monitoringService);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(MonitoringController::schedule, "/api/v1/monitoring/schedule", drogon::Get);
        ADD_METHOD_TO(MonitoringController::rapidChange, "/api/v1/monitoring/rapid-change/{project_id}", drogon::Post);
        METHOD_LIST_END

        /**
         * @brief GET /api/v1/monitoring/schedule?month=&rainfall_mm= 特殊时期监测调度建议
         * @return 通过 callback 返回汛期/强降雨等场景的监测频次与应对建议
         */
        void schedule(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/monitoring/rapid-change/{project_id} 快速变化识别
         *
         * 两期 DSM 高程骤降识别 (边坡坍塌/渣体流失), 创建异步任务。
         * @param projectId 项目 id
         * @return 通过 callback 返回 201 与任务信息
         */
        void rapidChange(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback, int projectId);

    private:
        std::shared_ptr<services::MonitoringService> monitoringService_;
    };

}  // namespace soilwatercm::controllers
