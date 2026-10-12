#pragma once

#include <memory>

#include <drogon/HttpController.h>

#include "../dao/image_dao.h"
#include "../dao/task_dao.h"
#include "../services/preprocess_service.h"
#include "../services/three_d_service.h"

namespace soilwatercm::controllers
{

    /**
     * @brief 影像加工处理 HTTP 控制器
     *
     * 提供预处理/镶嵌/三维/InSAR/地形水文分析等接口, 路由前缀 /api/v1/processing。
     */
    class ProcessingController : public drogon::HttpController<ProcessingController, false>
    {
    public:
        ProcessingController() = default;

        /**
         * @brief 构造, 注入预处理服务
         * @param preprocessService 影像预处理服务
         */
        explicit ProcessingController(std::shared_ptr<services::PreprocessService> preprocessService);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(ProcessingController::preprocess, "/api/v1/processing/preprocess", drogon::Post);
        ADD_METHOD_TO(ProcessingController::mosaic, "/api/v1/processing/mosaic", drogon::Post);
        ADD_METHOD_TO(ProcessingController::preprocessById, "/api/v1/processing/preprocess/{image_id}", drogon::Post);
        ADD_METHOD_TO(ProcessingController::threeD, "/api/v1/processing/three-d/{project_id}", drogon::Post);
        ADD_METHOD_TO(ProcessingController::insar, "/api/v1/processing/insar/{project_id}", drogon::Post);
        ADD_METHOD_TO(ProcessingController::changeDetection, "/api/v1/processing/change-detection/{project_id}",
                      drogon::Post);
        ADD_METHOD_TO(ProcessingController::terrain, "/api/v1/processing/terrain/{project_id}", drogon::Post);
        ADD_METHOD_TO(ProcessingController::watershed, "/api/v1/processing/watershed/{project_id}", drogon::Post);
        ADD_METHOD_TO(ProcessingController::inundation, "/api/v1/processing/inundation/{project_id}", drogon::Post);
        ADD_METHOD_TO(ProcessingController::viewshed, "/api/v1/processing/viewshed/{project_id}", drogon::Post);
        ADD_METHOD_TO(ProcessingController::surveySlopeFarmland,
                      "/api/v1/processing/survey/slope-farmland/{project_id}", drogon::Post);
        ADD_METHOD_TO(ProcessingController::surveyCheckDams, "/api/v1/processing/survey/check-dams/{project_id}",
                      drogon::Post);
        ADD_METHOD_TO(ProcessingController::surfaceDiscriminate, "/api/v1/processing/surface-discriminate/{project_id}",
                      drogon::Post);
        ADD_METHOD_TO(ProcessingController::thematicRaster, "/api/v1/processing/thematic/raster", drogon::Post);
        ADD_METHOD_TO(ProcessingController::thematicVector, "/api/v1/processing/thematic/vector", drogon::Post);
        ADD_METHOD_TO(ProcessingController::insarBackendStatus, "/api/v1/processing/insar/backend-status", drogon::Get);
        ADD_METHOD_TO(ProcessingController::gisBackendStatus, "/api/v1/processing/gis/backend-status", drogon::Get);
        ADD_METHOD_TO(ProcessingController::terrainResults, "/api/v1/processing/terrain/results", drogon::Get);
        METHOD_LIST_END

        /**
         * @brief POST /api/v1/processing/preprocess 批量预处理
         * @return 通过 callback 返回 202 与任务信息; 按 image_ids 提交任务
         */
        void preprocess(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/processing/mosaic 影像镶嵌
         * @return 通过 callback 返回 202 与任务信息
         */
        void mosaic(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/processing/preprocess/{image_id} 单影像预处理
         * @param imageId 影像 id
         * @return 通过 callback 返回任务信息; 支持 rad_method/dst_crs/dem_path 参数
         */
        void preprocessById(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback, int imageId);

        /**
         * @brief POST /api/v1/processing/three-d/{project_id} 三维建模
         * @param projectId 项目 id
         * @return 通过 callback 返回任务信息; 项目最新 DEM 转 3D Tiles, 后台任务
         */
        void threeD(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    int projectId);

        /**
         * @brief POST /api/v1/processing/insar/{project_id} InSAR 形变监测任务
         * @param projectId 项目 id
         * @return 通过 callback 返回任务信息; 后端依赖未注入, 任务登记后置失败
         */
        void insar(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                   int projectId);

        /**
         * @brief POST /api/v1/processing/change-detection/{project_id} 双期影像变化检测任务
         * @param projectId 项目 id
         * @return 通过 callback 返回任务信息; 算法未实现, 当前为占位执行
         */
        void changeDetection(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback, int projectId);

        /**
         * @brief POST /api/v1/processing/terrain/{project_id} 地形水文分析任务
         * @param projectId 项目 id
         * @return 通过 callback 返回任务信息; 依赖 SAGA, 当前为占位执行
         */
        void terrain(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     int projectId);

        /**
         * @brief POST /api/v1/processing/watershed/{project_id} 卡口点汇水区提取任务
         * @param projectId 项目 id
         * @return 通过 callback 返回任务信息; 依赖 D8, 当前为占位执行
         */
        void watershed(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback, int projectId);

        /**
         * @brief POST /api/v1/processing/inundation/{project_id} 按水位淹没分析任务
         * @param projectId 项目 id
         * @return 通过 callback 返回任务信息; 基于项目 DEM 真实执行
         */
        void inundation(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback, int projectId);

        /**
         * @brief POST /api/v1/processing/viewshed/{project_id} 视点通视分析任务
         * @param projectId 项目 id
         * @return 通过 callback 返回任务信息; 基于项目 DEM 真实执行
         */
        void viewshed(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                      int projectId);

        /**
         * @brief POST /api/v1/processing/survey/slope-farmland/{project_id} 坡耕地调查任务
         * @param projectId 项目 id
         * @return 通过 callback 返回任务信息; 算法未实现, 当前为占位执行
         */
        void surveySlopeFarmland(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback, int projectId);

        /**
         * @brief POST /api/v1/processing/survey/check-dams/{project_id} 淤地坝核查任务
         * @param projectId 项目 id
         * @return 通过 callback 返回任务信息; 算法未实现, 当前为占位执行
         */
        void surveyCheckDams(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback, int projectId);

        /**
         * @brief POST /api/v1/processing/surface-discriminate/{project_id} 地表光谱判别任务
         * @param projectId 项目 id
         * @return 通过 callback 返回任务信息; 算法未实现, 当前为占位执行
         */
        void surfaceDiscriminate(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback, int projectId);

        /**
         * @brief POST /api/v1/processing/thematic/raster 栅格专题图渲染任务
         * @return 通过 callback 返回任务信息; 渲染未实现, 当前为占位执行
         */
        void thematicRaster(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/processing/thematic/vector 矢量专题图渲染任务
         * @return 通过 callback 返回任务信息; 渲染未实现, 当前为占位执行
         */
        void thematicVector(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/processing/insar/backend-status InSAR 工具可用性
         * @return 通过 callback 返回检测结果 (isce2/snap 等外部工具)
         */
        void insarBackendStatus(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/processing/gis/backend-status GIS 工具可用性
         * @return 通过 callback 返回检测结果 (qgis/grass/saga/otb 外部工具)
         */
        void gisBackendStatus(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/processing/terrain/results 地形分析成果列表
         * @return 通过 callback 返回成果列表; 成果未实现, 当前恒为空数组
         */
        void terrainResults(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    private:
        std::shared_ptr<services::PreprocessService> preprocessService_;

        // 启动时未注入的依赖, 按全局配置惰性构建
        std::shared_ptr<repository::PostgresRepository> sharedRepo();
        std::shared_ptr<dao::TaskDao> taskDao();
        std::shared_ptr<dao::ImageDao> imageDao();
        std::shared_ptr<services::ThreeDService> threeDService();
        std::shared_ptr<repository::PostgresRepository> repo_;
        std::shared_ptr<dao::TaskDao> taskDao_;
        std::shared_ptr<dao::ImageDao> imageDao_;
        std::shared_ptr<services::ThreeDService> threeDService_;

        /// @brief 建 pending 任务记录; params.task_type 区分 TaskType 枚举覆盖不到的业务类型
        models::Task createTaskRecord(models::TaskType type, int projectId, int periodId, const Json::Value& taskParams,
                                      const std::string& taskType);

        /// @brief 任务实体 -> JSON 响应字段
        static Json::Value taskToJson(const models::Task& task);

        /// @brief 后台占位执行体: 写占位结果并回写任务状态
        void runPlaceholderTask(int taskId, std::string outputDir, std::string taskType);

        /// @brief 后台执行: 按水位淹没分析 (基于项目 DEM)
        void runInundationTask(int taskId, std::string demPath, double waterLevel);

        /// @brief 后台执行: 视点通视分析 (基于项目 DEM)
        void runViewshedTask(int taskId, std::string demPath, double obsLon, double obsLat, double obsHeight,
                             double radiusM);

        /// @brief 取参数: JSON body 优先, 其次 query
        static Json::Value param(const drogon::HttpRequestPtr& req, const std::string& key);

        /// @brief 取字符串参数; null/缺失返回默认值, 不抛 jsoncpp 类型异常
        static std::string strParam(const drogon::HttpRequestPtr& req, const std::string& key,
                                    const std::string& fallback = "");

        /// @brief 取数值参数; null/缺失返回默认值, 不抛 jsoncpp 类型异常
        static double numParam(const drogon::HttpRequestPtr& req, const std::string& key, double fallback = 0.0);
    };

}  // namespace soilwatercm::controllers
