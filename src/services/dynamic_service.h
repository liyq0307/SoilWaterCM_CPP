#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "../dao/disturbance_dao.h"
#include "../dao/erosion_result_dao.h"
#include "../dao/image_dao.h"
#include "../dao/indicator_dao.h"
#include "../dao/measure_dao.h"
#include "../dao/monitoring_period_dao.h"
#include "../dao/task_dao.h"
#include "../dao/vegetation_coverage_dao.h"
#include "../models/task.h"

namespace soilwatercm::services
{

    /**
     * @brief 动态变化图服务 (移植自 Python endpoints/dynamic.py)
     *
     * period-layers: 各期专题图层 (PNG 渲染未移植, 以数据可用性聚合代替);
     * timeseries: 多指标跨期趋势 (返回 JSON 数据, 不渲染 PNG);
     * comparison: 两期成果指标对比 (返回 JSON 聚合, 不渲染 PNG);
     * animation: 逐期变化动画 (异步任务占位); file: results/dynamic 下文件下载。
     */
    class DynamicService
    {
    public:
        /**
         * @brief 构造, 注入监测期/影像/任务/指标与各成果 DAO
         * @param periodDao 监测期 DAO
         * @param imageDao 影像 DAO
         * @param taskDao 任务 DAO
         * @param indicatorDao 指标 DAO (趋势取指标值)
         * @param fvcDao 植被覆盖度 DAO (图层数据源检查)
         * @param erosionDao 侵蚀结果 DAO (图层数据源检查)
         * @param disturbanceDao 扰动 DAO (图层数据源检查)
         * @param measureDao 措施 DAO (图层数据源检查)
         */
        DynamicService(std::shared_ptr<dao::MonitoringPeriodDao> periodDao, std::shared_ptr<dao::ImageDao> imageDao,
                       std::shared_ptr<dao::TaskDao> taskDao, std::shared_ptr<dao::IndicatorDao> indicatorDao,
                       std::shared_ptr<dao::VegetationCoverageDao> fvcDao,
                       std::shared_ptr<dao::ErosionResultDao> erosionDao,
                       std::shared_ptr<dao::DisturbanceDao> disturbanceDao, std::shared_ptr<dao::MeasureDao> measureDao);

        /**
         * @brief 各期专题图层
         *
         * 全部期数据源齐备则直接返回成功结果 (缓存命中路径),
         * 否则复用同项目同图层活跃任务或创建新任务。
         * @param projectId 项目 id
         * @param layer 图层名 (fvc/coverage/erosion/landuse/disturbance/measure)
         * @return 结果或任务 JSON
         * @throw ValidationException 图层名未知或 project_id 无效时
         */
        nlohmann::json periodLayers(int projectId, const std::string& layer);

        /**
         * @brief 多指标趋势 (对齐 Python build_multi_trend)
         * @param params 须含 project_id
         * @return {project_id, series:[{indicator_code, indicator_name, dimension, unit,
         *          points:[{period_code, period_date, value, label}]}]}
         * @throw ValidationException 缺 project_id 或该项目暂无指标数据时
         */
        nlohmann::json timeSeries(const nlohmann::json& params);

        /**
         * @brief 两期对比 (mode=side/diff)
         * @param params 须含 project_id / code_a / code_b / mode
         * @return {layer, mode, period_a, period_b, delta} 聚合 JSON
         * @throw ValidationException 缺参数 / 未知 mode / 期次不存在时
         */
        nlohmann::json comparison(const nlohmann::json& params);

        /**
         * @brief 逐期变化动画 (异步)
         * @param params 须含 project_id
         * @return 任务 JSON (创建 change_animation 任务, 后台简化执行)
         * @throw ValidationException 缺 project_id 时
         */
        nlohmann::json animation(const nlohmann::json& params);

        /**
         * @brief 校验并返回 results/dynamic 下的文件路径
         * @param params 须含 path (相对路径)
         * @return 文件绝对路径
         * @throw ValidationException 缺 path 或路径越界时
         * @throw FileNotFoundException 文件不存在时
         */
        std::string getFile(const nlohmann::json& params);

        /// @brief 动态输出根目录 (results/dynamic)
        static std::string dynamicRoot();

        /**
         * @brief Task -> JSON (对齐 TaskRead 字段)
         * @param task 任务记录
         * @return JSON
         */
        static nlohmann::json taskToJson(const models::Task& task);

    private:
        // 图层名合法性校验 (fvc/coverage/erosion/landuse/disturbance/measure)
        static void checkLayer(const std::string& layer);

        // 某期该图层是否有底层数据
        bool periodLayerSourceExists(int projectId, int periodId, const std::string& layer);

        // 创建异步任务记录 (task_type 写入 params 以区分本项目 TaskType 枚举)
        int createAsyncTask(const std::string& taskType, int projectId, const nlohmann::json& taskParams);

        // 后台执行体 (渲染算法未移植): 写占位结果 + 回写任务状态
        void runRenderPlaceholder(int taskId, std::string outputDir, std::string taskType);

        std::shared_ptr<dao::MonitoringPeriodDao> periodDao_;
        std::shared_ptr<dao::ImageDao> imageDao_;
        std::shared_ptr<dao::TaskDao> taskDao_;
        std::shared_ptr<dao::IndicatorDao> indicatorDao_;
        std::shared_ptr<dao::VegetationCoverageDao> fvcDao_;
        std::shared_ptr<dao::ErosionResultDao> erosionDao_;
        std::shared_ptr<dao::DisturbanceDao> disturbanceDao_;
        std::shared_ptr<dao::MeasureDao> measureDao_;
    };

}  // namespace soilwatercm::services
