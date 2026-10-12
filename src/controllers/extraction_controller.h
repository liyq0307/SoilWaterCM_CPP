#pragma once

#include <memory>
#include <optional>

#include <drogon/HttpController.h>

#include "../dao/anomaly_dao.h"
#include "../dao/image_dao.h"
#include "../dao/task_dao.h"
#include "../services/disturbance_service.h"
#include "../services/erosion_service.h"
#include "../services/fvc_service.h"
#include "../services/measure_service.h"

namespace soilwatercm::controllers
{

    /**
     * @brief 遥感信息提取 HTTP 控制器
     *
     * 提供 FVC/水蚀/扰动/水保措施解译与图斑管理, 路由前缀 /api/v1/extraction。
     */
    class ExtractionController : public drogon::HttpController<ExtractionController, false>
    {
    public:
        ExtractionController() = default;

        /**
         * @brief 构造, 注入各提取服务
         * @param fvcService 植被覆盖度提取服务
         * @param erosionService 土壤侵蚀计算服务
         * @param disturbanceService 扰动图斑服务
         * @param measureService 水保措施服务
         */
        ExtractionController(std::shared_ptr<services::FvcService> fvcService,
                             std::shared_ptr<services::ErosionService> erosionService,
                             std::shared_ptr<services::DisturbanceService> disturbanceService,
                             std::shared_ptr<services::MeasureService> measureService);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(ExtractionController::extractFvc, "/api/v1/extraction/fvc", drogon::Post);
        ADD_METHOD_TO(ExtractionController::extractErosion, "/api/v1/extraction/erosion", drogon::Post);
        ADD_METHOD_TO(ExtractionController::extractDisturbance, "/api/v1/extraction/disturbance", drogon::Post);
        ADD_METHOD_TO(ExtractionController::extractMeasures, "/api/v1/extraction/measures", drogon::Post);
        ADD_METHOD_TO(ExtractionController::listDisturbances, "/api/v1/extraction/disturbances", drogon::Get);
        ADD_METHOD_TO(ExtractionController::createDisturbance, "/api/v1/extraction/disturbances", drogon::Post);
        ADD_METHOD_TO(ExtractionController::autoExtractDisturbances, "/api/v1/extraction/disturbances/auto-extract",
                      drogon::Post);
        ADD_METHOD_TO(ExtractionController::listMeasures, "/api/v1/extraction/measures", drogon::Get);
        ADD_METHOD_TO(ExtractionController::autoInterpretMeasures, "/api/v1/extraction/measures/auto-interpret",
                      drogon::Post);
        ADD_METHOD_TO(ExtractionController::listLanduse, "/api/v1/extraction/landuse", drogon::Get);
        ADD_METHOD_TO(ExtractionController::listSegments, "/api/v1/extraction/segments", drogon::Get);
        ADD_METHOD_TO(ExtractionController::listSpoilDumps, "/api/v1/extraction/spoil-dumps", drogon::Get);
        ADD_METHOD_TO(ExtractionController::autoDetectSpoilDumps, "/api/v1/extraction/spoil-dumps/auto-detect",
                      drogon::Post);
        ADD_METHOD_TO(ExtractionController::submitClassify, "/api/v1/extraction/classify", drogon::Post);
        ADD_METHOD_TO(ExtractionController::submitSegment, "/api/v1/extraction/segment", drogon::Post);
        ADD_METHOD_TO(ExtractionController::trainChange, "/api/v1/extraction/change/train", drogon::Post);
        ADD_METHOD_TO(ExtractionController::predictChange, "/api/v1/extraction/change/predict", drogon::Post);
        ADD_METHOD_TO(ExtractionController::latestTrainedModel, "/api/v1/extraction/trained-model/latest",
                      drogon::Get);
        ADD_METHOD_TO(ExtractionController::finetuneResources, "/api/v1/extraction/finetune/resources", drogon::Get);
        ADD_METHOD_TO(ExtractionController::reviewCandidates, "/api/v1/extraction/disturbances/review-candidates",
                      drogon::Get);
        ADD_METHOD_TO(ExtractionController::learningStats, "/api/v1/extraction/disturbances/learning-stats",
                      drogon::Get);
        ADD_METHOD_TO(ExtractionController::reviewDisturbance, "/api/v1/extraction/disturbances/{id}/review",
                      drogon::Post);
        METHOD_LIST_END

        /**
         * @brief POST /api/v1/extraction/fvc 提交植被覆盖度提取任务
         * @return 通过 callback 返回 202 与任务信息
         */
        void extractFvc(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/extraction/erosion 提交土壤侵蚀计算任务
         * @return 通过 callback 返回 202 与任务信息
         */
        void extractErosion(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/extraction/disturbance 提交扰动提取任务
         * @return 通过 callback 返回 202 与任务信息
         */
        void extractDisturbance(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/extraction/measures 措施解译或人工建措施
         * @return 通过 callback 返回结果; body 带 geom_geojson 时创建措施记录 (201), 否则提交解译任务 (202)
         */
        void extractMeasures(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/extraction/disturbances?project_id= 列出项目扰动图斑
         * @return 通过 callback 返回图斑列表
         */
        void listDisturbances(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/extraction/disturbances 人工创建扰动图斑记录
         * @return 通过 callback 返回 201 与新记录
         */
        void createDisturbance(const drogon::HttpRequestPtr& req,
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/extraction/disturbances/auto-extract 自动提取扰动图斑
         * @return 通过 callback 返回任务信息; 基于双期影像, 后台任务
         */
        void autoExtractDisturbances(const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/extraction/measures?project_id= 列出项目水保措施
         * @return 通过 callback 返回措施列表
         */
        void listMeasures(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/extraction/measures/auto-interpret 半自动解译水保措施
         * @return 通过 callback 返回任务信息; 基于影像, 后台任务
         */
        void autoInterpretMeasures(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/extraction/landuse 土地利用分类成果列表
         * @return 通过 callback 返回成果列表; 成果未实现, 当前恒为空数组
         */
        void listLanduse(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/extraction/segments 影像分割成果列表
         * @return 通过 callback 返回成果列表; 成果未实现, 当前恒为空数组
         */
        void listSegments(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/extraction/spoil-dumps 弃渣场列表
         * @return 通过 callback 返回弃渣场列表; 成果未实现, 当前恒为空数组
         */
        void listSpoilDumps(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/extraction/spoil-dumps/auto-detect 弃渣场自动识别
         * @return 通过 callback 返回任务信息; 算法未实现, 任务登记后置失败
         */
        void autoDetectSpoilDumps(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/extraction/classify 土地利用分类任务
         * @return 通过 callback 返回任务信息; AI 后端未接入, 当前为占位实现
         */
        void submitClassify(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/extraction/segment 影像分割任务
         * @return 通过 callback 返回任务信息; AI 后端未接入, 当前为占位实现
         */
        void submitSegment(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/extraction/change/train 变化检测模型训练
         * @return 通过 callback 返回任务信息; AI 后端未接入, 当前为占位实现
         */
        void trainChange(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/extraction/change/predict 变化检测模型推理
         * @return 通过 callback 返回任务信息; AI 后端未接入, 当前为占位实现
         */
        void predictChange(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/extraction/trained-model/latest 最新训练模型信息
         * @return 通过 callback 返回模型信息; 无模型注册表, 当前返回空占位
         */
        void latestTrainedModel(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/extraction/finetune/resources 微调数据与权重盘点
         * @return 通过 callback 返回清单; 能力未实现, 当前恒为空清单
         */
        void finetuneResources(const drogon::HttpRequestPtr& req,
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/extraction/disturbances/review-candidates 复核候选清单
         * @return 通过 callback 返回低置信度图斑列表
         */
        void reviewCandidates(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/extraction/disturbances/learning-stats 复核学习统计
         * @return 通过 callback 返回扰动图斑复核统计
         */
        void learningStats(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/extraction/disturbances/{id}/review 复核图斑
         * @param id 扰动图斑 id
         * @return 通过 callback 返回复核结果; confirm 提升置信度, false_alarm 转异常留档
         */
        void reviewDisturbance(const drogon::HttpRequestPtr& req,
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

    private:
        std::shared_ptr<services::FvcService> fvcService_;
        std::shared_ptr<services::ErosionService> erosionService_;
        std::shared_ptr<services::DisturbanceService> disturbanceService_;
        std::shared_ptr<services::MeasureService> measureService_;

        // 启动时未注入的依赖, 按全局配置惰性构建
        std::shared_ptr<repository::PostgresRepository> sharedRepo();
        std::shared_ptr<dao::TaskDao> taskDao();
        std::shared_ptr<dao::ImageDao> imageDao();
        std::shared_ptr<dao::AnomalyDao> anomalyDao();
        std::shared_ptr<repository::PostgresRepository> repo_;
        std::shared_ptr<dao::TaskDao> taskDao_;
        std::shared_ptr<dao::ImageDao> imageDao_;
        std::shared_ptr<dao::AnomalyDao> anomalyDao_;

        /// @brief 建 pending 任务; params.task_type 区分业务类型, AI 类任务后台置失败并说明原因
        models::Task createAiTask(const std::string& taskType, int projectId, int periodId,
                                  const Json::Value& taskParams);

        /// @brief AI 训练/预测类端点统一入口: 无 AI 运行时, 建任务后后台标记 failed 并写说明
        void submitAiPlaceholder(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                 const std::string& taskType);

        /// @brief 扰动图斑实体 -> JSON 响应字段
        static Json::Value disturbanceToJson(const models::Disturbance& d);

        /// @brief 水保措施实体 -> JSON 响应字段
        static Json::Value measureToJson(const models::Measure& m);
    };

}  // namespace soilwatercm::controllers
