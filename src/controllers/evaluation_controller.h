#pragma once

#include <drogon/HttpController.h>

#include "../services/evaluation_service.h"

namespace soilwatercm::controllers
{

    /**
     * @brief 水土保持评价 HTTP 控制器
     *
     * 提供评价任务提交与合规/风险评分查询, 路由前缀 /api/v1/evaluation。
     */
    class EvaluationController : public drogon::HttpController<EvaluationController, false>
    {
    public:
        EvaluationController() = default;

        /**
         * @brief 构造, 注入评价服务
         * @param evaluationService 水土保持评价服务
         */
        explicit EvaluationController(std::shared_ptr<services::EvaluationService> evaluationService);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(EvaluationController::evaluate, "/api/v1/evaluation", drogon::Post);
        ADD_METHOD_TO(EvaluationController::evaluate, "/api/v1/evaluation/run", drogon::Post);
        ADD_METHOD_TO(EvaluationController::getResultByQuery, "/api/v1/evaluation/result", drogon::Get);
        ADD_METHOD_TO(EvaluationController::getResult, "/api/v1/evaluation/{id}", drogon::Get);
        METHOD_LIST_END

        /**
         * @brief POST /api/v1/evaluation 或 /run 提交评价任务
         * @return 通过 callback 返回 202 与任务信息; 按监测期异步执行
         */
        void evaluate(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/evaluation/result?period_id= 按监测期查询评价结果
         * @return 通过 callback 返回评价结果
         */
        void getResultByQuery(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/evaluation/{id} 按监测期 id 查询评价结果
         * @param id 监测期 id
         * @return 通过 callback 返回评价结果
         */
        void getResult(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

    private:
        std::shared_ptr<services::EvaluationService> evaluationService_;
    };

}  // namespace soilwatercm::controllers
