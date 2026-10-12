#pragma once

#include <drogon/HttpController.h>

#include "../services/dynamic_service.h"

namespace soilwatercm::controllers
{

    /**
     * @brief 动态变化分析 HTTP 控制器
     *
     * 提供各期图层、时间序列、两期对比与变化动画, 路由前缀 /api/v1/dynamic。
     */
    class DynamicController : public drogon::HttpController<DynamicController, false>
    {
    public:
        DynamicController() = default;

        /**
         * @brief 构造, 注入动态变化分析服务
         * @param dynamicService 动态变化分析服务
         */
        explicit DynamicController(std::shared_ptr<services::DynamicService> dynamicService);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(DynamicController::periodLayers, "/api/v1/dynamic/period-layers", drogon::Get);
        ADD_METHOD_TO(DynamicController::timeSeries, "/api/v1/dynamic/timeseries", drogon::Post);
        ADD_METHOD_TO(DynamicController::comparison, "/api/v1/dynamic/comparison", drogon::Post);
        ADD_METHOD_TO(DynamicController::animation, "/api/v1/dynamic/animation", drogon::Post);
        ADD_METHOD_TO(DynamicController::downloadFile, "/api/v1/dynamic/file", drogon::Get);
        METHOD_LIST_END

        /**
         * @brief GET /api/v1/dynamic/period-layers?project_id=&layer= 各监测期专题图层
         * @return 通过 callback 返回图层列表 (fvc/erosion 等)
         */
        void periodLayers(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/dynamic/timeseries 多指标跨期趋势数据
         * @return 通过 callback 返回趋势序列
         */
        void timeSeries(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/dynamic/comparison 两期成果指标对比
         * @return 通过 callback 返回对比结果 (mode=side/diff)
         */
        void comparison(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/dynamic/animation 创建逐期变化动画任务
         * @return 通过 callback 返回 201 与任务信息
         */
        void animation(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/dynamic/file?path= 下载文件
         * @return 通过 callback 返回 dynamic 输出目录下的文件
         */
        void downloadFile(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    private:
        std::shared_ptr<services::DynamicService> dynamicService_;
    };

}  // namespace soilwatercm::controllers
