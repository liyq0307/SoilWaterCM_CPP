#pragma once

#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../dao/measure_dao.h"
#include "../dao/task_dao.h"
#include "../infrastructure/gdal/gdal_wrapper.h"
#include "../infrastructure/gdal/vector_processor.h"
#include "../models/measure.h"

namespace soilwatercm::services
{

    /** @brief 水保措施解译入参 */
    struct MeasureParams
    {
        int period_id;                              ///< 期次 id
        std::string image_path;                     ///< 影像路径
        std::string output_dir;                     ///< 输出目录
        std::optional<std::string> measure_type;    ///< 措施类型; 为空则不限
    };

    /** @brief 措施解译结果 */
    struct MeasureResult
    {
        bool success = false;                 ///< 是否成功
        std::string message;                  ///< 结果说明/失败原因
        std::vector<models::Measure> measures;///< 措施列表
        int total_count = 0;                  ///< 措施总数
        std::optional<int> task_id;           ///< 关联任务 id
    };

    /**
     * @brief 水土保持措施解译服务
     *
     * 从影像检测措施地物并矢量化, 计算面积/长度属性后入库;
     * 也支持手动建措施记录与按期/项目查询。
     */
    class MeasureService
    {
    public:
        /**
         * @brief 构造, 注入措施/任务 DAO、GDAL 与矢量处理器
         * @param measureDao 措施 DAO (入库)
         * @param taskDao 任务 DAO (进度回写)
         * @param gdal GDAL 封装 (面积/长度计算)
         * @param vectorProcessor 矢量处理器 (地物检测)
         */
        explicit MeasureService(std::shared_ptr<dao::MeasureDao> measureDao, std::shared_ptr<dao::TaskDao> taskDao,
                                std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal,
                                std::shared_ptr<infrastructure::gdal::VectorProcessor> vectorProcessor);

        ~MeasureService() = default;

        /**
         * @brief 异步提交措施解译任务
         * @param params 解译入参
         * @return 结果 future (建任务记录后后台执行)
         */
        std::future<MeasureResult> submitMeasureTask(const MeasureParams& params);

        /**
         * @brief 解译主流程
         *
         * 检测 -> 计算属性 -> 入库, 回写任务进度。
         * @param params 解译入参
         * @param taskId 关联任务 id
         * @return 解译结果 (异常以失败结果返回)
         */
        MeasureResult executeMeasure(const MeasureParams& params, int taskId);

        /**
         * @brief 从影像检测措施地物
         *
         * 当前为简化实现: 检测结果统一标为谷坊类型。
         * @param imagePath 影像路径
         * @param measureType 措施类型 (当前未参与分类)
         * @return 措施列表 (仅几何, 属性待后续计算)
         */
        std::vector<models::Measure> detectMeasures(const std::string& imagePath, const std::string& measureType = "");

        /**
         * @brief 由几何 WKT 计算措施面积与长度 (原地回写 measure)
         * @param measure 措施记录 (几何为空则跳过)
         */
        void calculateMeasureAttributes(models::Measure& measure);

        /**
         * @brief FVC 分级
         * @param fvcValue 覆盖度值 [0,1]
         * @return 等级 1-4 (按 0.3/0.5/0.7 分档)
         */
        double classifyVegetationCoverage(double fvcValue);

        /**
         * @brief 按期次查措施
         * @param periodId 期次 id
         * @return 措施列表
         */
        std::vector<models::Measure> getMeasuresByPeriod(int periodId);

        /**
         * @brief 按项目列措施 (可选期次过滤), 对齐 Python GET /extraction/measures
         * @param projectId 项目 id
         * @param periodId 期次 id; 为空则全期次
         * @return 措施列表
         */
        std::vector<models::Measure> getMeasuresByProject(int projectId, std::optional<int> periodId);

        /**
         * @brief 手动创建措施记录, 对齐 Python POST /extraction/measures
         * @param measure 措施记录
         * @return 新记录 id
         */
        int createMeasure(const models::Measure& measure);

    private:
        std::shared_ptr<dao::MeasureDao> measureDao_;
        std::shared_ptr<dao::TaskDao> taskDao_;
        std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal_;
        std::shared_ptr<infrastructure::gdal::VectorProcessor> vectorProcessor_;

        void updateTaskProgress(int taskId, int progress, const std::string& message);
    };

}  // namespace soilwatercm::services
