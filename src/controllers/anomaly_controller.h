#pragma once

#include <drogon/HttpController.h>

#include "../services/anomaly_service.h"

namespace soilwatercm::controllers
{

    /**
     * @brief 遥感监测异常管理 HTTP 控制器
     *
     * 提供异常查询、检测、更新与处置流转, 路由前缀 /api/v1/anomalies。
     */
    class AnomalyController : public drogon::HttpController<AnomalyController, false>
    {
    public:
        AnomalyController() = default;

        /**
         * @brief 构造, 注入异常服务
         * @param anomalyService 异常管理服务
         */
        explicit AnomalyController(std::shared_ptr<services::AnomalyService> anomalyService);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(AnomalyController::listAnomalies, "/api/v1/anomalies", drogon::Get);
        ADD_METHOD_TO(AnomalyController::detect, "/api/v1/anomalies/detect", drogon::Post);
        ADD_METHOD_TO(AnomalyController::getAnomaly, "/api/v1/anomalies/{id}", drogon::Get);
        ADD_METHOD_TO(AnomalyController::updateAnomaly, "/api/v1/anomalies/{id}", drogon::Put);
        ADD_METHOD_TO(AnomalyController::assign, "/api/v1/anomalies/{id}/assign", drogon::Post);
        ADD_METHOD_TO(AnomalyController::dispatch, "/api/v1/anomalies/{id}/dispatch", drogon::Post);
        ADD_METHOD_TO(AnomalyController::notify, "/api/v1/anomalies/{id}/notify", drogon::Post);
        ADD_METHOD_TO(AnomalyController::resolve, "/api/v1/anomalies/{id}/resolve", drogon::Post);
        METHOD_LIST_END

        /**
         * @brief GET /api/v1/anomalies?project_id= 按项目列出异常
         * @return 通过 callback 返回异常列表
         */
        void listAnomalies(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/anomalies/detect 执行异常检测
         * @return 通过 callback 返回检测结果; 按指定项目/监测期
         */
        void detect(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/anomalies/{id} 查询单条异常详情
         * @param id 异常 id
         * @return 通过 callback 返回异常记录
         */
        void getAnomaly(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief PUT /api/v1/anomalies/{id} 更新异常
         * @param id 异常 id
         * @return 通过 callback 返回更新后的记录; 可改状态/等级/描述/责任人等字段
         */
        void updateAnomaly(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief POST /api/v1/anomalies/{id}/assign 指派异常
         * @param id 异常 id
         * @return 通过 callback 返回指派结果
         */
        void assign(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    int id);

        /**
         * @brief POST /api/v1/anomalies/{id}/dispatch 派单处置
         * @param id 异常 id
         * @return 通过 callback 返回处置结果; 状态置为已指派
         */
        void dispatch(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief POST /api/v1/anomalies/{id}/notify 通知相关人
         * @param id 异常 id
         * @return 通过 callback 返回通知结果; 当前为占位实现, 仅记录日志
         */
        void notify(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    int id);

        /**
         * @brief POST /api/v1/anomalies/{id}/resolve 异常结案
         * @param id 异常 id
         * @return 通过 callback 返回结案结果; 填写处置措施
         */
        void resolve(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     int id);

    private:
        std::shared_ptr<services::AnomalyService> anomalyService_;
    };

}  // namespace soilwatercm::controllers
