#pragma once

#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../dao/disturbance_dao.h"
#include "../dao/task_dao.h"
#include "../infrastructure/external_tools/grass_interface.h"
#include "../infrastructure/external_tools/otb_interface.h"
#include "../infrastructure/gdal/gdal_wrapper.h"
#include "../infrastructure/gdal/raster_processor.h"
#include "../infrastructure/gdal/vector_processor.h"
#include "../models/disturbance.h"

namespace soilwatercm::services
{

    /** @brief 扰动图斑提取入参 */
    struct DisturbanceParams
    {
        int period_id;                        ///< 期次 id
        std::string before_image_path;        ///< 前时相影像路径
        std::string after_image_path;         ///< 后时相影像路径
        std::string output_dir;               ///< 输出目录
        double min_area = 0.1;                ///< 最小图斑面积 (公顷), 小于则过滤
        double confidence_threshold = 0.7;    ///< 置信度阈值
    };

    /** @brief 扰动提取结果 */
    struct DisturbanceResult
    {
        bool success = false;                          ///< 是否成功
        std::string message;                           ///< 结果说明/失败原因
        std::vector<models::Disturbance> disturbances; ///< 提取的图斑列表
        int total_count = 0;                           ///< 图斑总数
        double total_area = 0.0;                       ///< 总面积 (公顷)
        std::optional<int> task_id;                    ///< 关联任务 id
    };

    /**
     * @brief 扰动图斑提取服务
     *
     * 前后时相影像做变化检测, 分类二值化后栅格转矢量,
     * 过滤小图斑入库; 支持手动建图斑与复核更新/删除。
     */
    class DisturbanceService
    {
    public:
        /**
         * @brief 构造, 注入扰动/任务 DAO、GDAL、栅格/矢量处理器与 OTB/GRASS 接口
         * @param disturbanceDao 扰动 DAO (入库)
         * @param taskDao 任务 DAO (进度回写)
         * @param gdal GDAL 封装
         * @param rasterProcessor 栅格处理器 (变化检测/二值化)
         * @param vectorProcessor 矢量处理器 (栅格转矢量)
         * @param otb OTB 接口 (变化检测)
         * @param grass GRASS 接口 (形态学处理)
         */
        explicit DisturbanceService(std::shared_ptr<dao::DisturbanceDao> disturbanceDao,
                                    std::shared_ptr<dao::TaskDao> taskDao,
                                    std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal,
                                    std::shared_ptr<infrastructure::gdal::RasterProcessor> rasterProcessor,
                                    std::shared_ptr<infrastructure::gdal::VectorProcessor> vectorProcessor,
                                    std::shared_ptr<infrastructure::OtbInterface> otb,
                                    std::shared_ptr<infrastructure::external_tools::GrassInterface> grass);

        ~DisturbanceService() = default;

        /**
         * @brief 异步提交提取任务
         * @param params 提取入参
         * @return 结果 future (建任务记录后在后台线程执行)
         */
        std::future<DisturbanceResult> submitDisturbanceTask(const DisturbanceParams& params);

        /**
         * @brief 提取主流程
         *
         * 变化检测 -> 分类 -> 矢量化 -> 面积过滤入库, 全程回写任务进度。
         * @param params 提取入参
         * @param taskId 关联任务 id
         * @return 提取结果 (异常以失败结果返回)
         */
        DisturbanceResult executeDisturbance(const DisturbanceParams& params, int taskId);

        /**
         * @brief 变化检测
         * @param beforePath 前时相影像路径
         * @param afterPath 后时相影像路径
         * @param outputPath 输出路径
         * @return 输出路径; OTB 可用则调 OTB, 否则 GDAL 逐像元求差值绝对值
         */
        std::string changeDetection(const std::string& beforePath, const std::string& afterPath,
                                    const std::string& outputPath);

        /**
         * @brief 扰动分类
         * @param changePath 变化图路径
         * @param outputPath 输出路径
         * @return 输出路径; 以变化图最大值的 30% 为阈值二值化 (1=扰动, 0=未扰动)
         */
        std::string classifyDisturbance(const std::string& changePath, const std::string& outputPath);

        /**
         * @brief 栅格转矢量, 得到候选扰动图斑几何
         * @param rasterPath 二值栅格路径
         * @return 图斑几何列表
         */
        std::vector<models::Geometry> rasterToVector(const std::string& rasterPath);

        /**
         * @brief 形态学处理 (opening/closing 等)
         * @param vectorPath 输入矢量路径
         * @param outputPath 输出路径
         * @param operation 操作类型
         * @return 结果路径; GRASS 不可用时原样返回输入路径
         */
        std::string morphologyProcess(const std::string& vectorPath, const std::string& outputPath,
                                      const std::string& operation = "opening");

        /**
         * @brief 保留面积 >= minArea 的图斑, 滤掉小图斑
         * @param disturbances 候选图斑列表
         * @param minArea 最小面积 (公顷)
         * @return 过滤后的图斑列表
         */
        std::vector<models::Disturbance> filterSmallPolygons(std::vector<models::Disturbance>& disturbances,
                                                             double minArea);

        /**
         * @brief 按期次查扰动图斑
         * @param periodId 期次 id
         * @return 图斑列表
         */
        std::vector<models::Disturbance> getDisturbancesByPeriod(int periodId);

        /**
         * @brief 按项目列扰动 (可选期次过滤), 对齐 Python GET /extraction/disturbances
         * @param projectId 项目 id
         * @param periodId 期次 id; 为空则全期次
         * @return 图斑列表
         */
        std::vector<models::Disturbance> getDisturbancesByProject(int projectId, std::optional<int> periodId);

        /**
         * @brief 手动创建扰动记录, 对齐 Python POST /extraction/disturbances
         * @param disturbance 图斑记录
         * @return 新记录 id
         */
        int createDisturbance(const models::Disturbance& disturbance);

        /**
         * @brief 按 id 读取扰动记录 (复核前取原图斑)
         * @param id 图斑主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::Disturbance> getDisturbanceById(int id);

        /**
         * @brief 更新扰动记录 (复核确认置信度等)
         * @param disturbance 记录 (须含 id)
         * @return 更新成功返回 true
         */
        bool updateDisturbance(const models::Disturbance& disturbance);

        /**
         * @brief 删除扰动记录 (误报复核移除)
         * @param id 图斑主键
         * @return 删除成功返回 true
         */
        bool deleteDisturbance(int id);

    private:
        std::shared_ptr<dao::DisturbanceDao> disturbanceDao_;
        std::shared_ptr<dao::TaskDao> taskDao_;
        std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal_;
        std::shared_ptr<infrastructure::gdal::RasterProcessor> rasterProcessor_;
        std::shared_ptr<infrastructure::gdal::VectorProcessor> vectorProcessor_;
        std::shared_ptr<infrastructure::OtbInterface> otb_;
        std::shared_ptr<infrastructure::external_tools::GrassInterface> grass_;

        void updateTaskProgress(int taskId, int progress, const std::string& message);
    };

}  // namespace soilwatercm::services
