#pragma once

#include <memory>

#include <drogon/HttpController.h>

#include "../services/ground_truth_service.h"

namespace soilwatercm::controllers
{

    /**
     * @brief 地面实测样点 HTTP 控制器
     *
     * 提供实测记录管理与遥感精度验证, 路由前缀 /api/v1/ground-truth。
     */
    class GroundTruthController : public drogon::HttpController<GroundTruthController, false>
    {
    public:
        GroundTruthController() = default;

        /**
         * @brief 构造, 注入实测样点服务
         * @param groundTruthService 地面实测样点服务
         */
        explicit GroundTruthController(std::shared_ptr<services::GroundTruthService> groundTruthService);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(GroundTruthController::listGroundTruth, "/api/v1/ground-truth", drogon::Get);
        ADD_METHOD_TO(GroundTruthController::createGroundTruth, "/api/v1/ground-truth", drogon::Post);
        ADD_METHOD_TO(GroundTruthController::verifyAccuracy, "/api/v1/ground-truth/{id}/verify-accuracy", drogon::Post);
        METHOD_LIST_END

        /**
         * @brief GET /api/v1/ground-truth?project_id=&sample_type=&page= 分页列出实测样点
         * @return 通过 callback 返回样点分页列表
         */
        void listGroundTruth(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/ground-truth 创建实测记录
         *
         * 位置支持 lng/lat 或 WKT。
         * @return 通过 callback 返回 201 与新记录
         */
        void createGroundTruth(const drogon::HttpRequestPtr& req,
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/ground-truth/{id}/verify-accuracy 精度验证
         *
         * 将实测值与对应遥感值比对, 计算误差并判定精度。
         * @param id 实测样点 id
         * @return 通过 callback 返回误差与精度判定
         */
        void verifyAccuracy(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

    private:
        std::shared_ptr<services::GroundTruthService> groundTruthService_;
    };

}  // namespace soilwatercm::controllers
