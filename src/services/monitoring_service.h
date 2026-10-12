#pragma once

#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "../dao/monitoring_period_dao.h"
#include "../dao/task_dao.h"
#include "../models/task.h"

namespace soilwatercm::services
{

    /**
     * @brief 特殊时期监测调度服务 (移植自 Python endpoints/monitoring.py +
     *        services/monitoring_schedule.py)
     *
     * 汛期加密频次 / 强降雨应急 / 冰雪数据替代 + 边坡坍塌/渣体流失快速识别任务。
     */
    class MonitoringService
    {
    public:
        /**
         * @brief 构造, 注入监测期与任务 DAO
         * @param periodDao 监测期 DAO
         * @param taskDao 任务 DAO (快速识别建任务)
         */
        MonitoringService(std::shared_ptr<dao::MonitoringPeriodDao> periodDao, std::shared_ptr<dao::TaskDao> taskDao);

        /**
         * @brief 特殊时期应对建议
         *
         * 汛期 (5-9 月) 间隔缩短至 10 天, 常规 30 天; 强降雨 >=50mm 启动应急;
         * 云雪占比 >10% 提示数据替代。
         * @param params month(1-12, 缺省当前月) / rainfall_mm / cloud_snow_ratio,
         *               project_id 可选
         * @return {month, flood_season, recommended_interval_days, emergency_monitoring,
         *         rainfall_mm, data_substitute_needed, cloud_snow_ratio, advice:[...]}
         */
        nlohmann::json schedule(const nlohmann::json& params);

        /**
         * @brief 快速变化识别
         *
         * 两期 DSM 高程骤降 -> 创建异步任务, 后台简化执行。
         * @param projectId 项目 id
         * @param params mode (slope_collapse/dump_loss)、dsm_before / dsm_after 路径
         * @return 任务 JSON
         * @throw ValidationException 缺 project_id / 未知 mode / 缺 DSM 路径时
         * @throw DatabaseException 创建任务失败时
         */
        nlohmann::json rapidChange(int projectId, const nlohmann::json& params);

        /**
         * @brief Task -> JSON (对齐 TaskRead 字段)
         * @param task 任务记录
         * @return JSON
         */
        static nlohmann::json taskToJson(const models::Task& task);

    private:
        // 后台执行体 (独立线程): DSM 差值算法未移植, 简化为建目录 + 回写任务状态
        void runRapidChangeTask(int taskId, std::string outputDir);

        std::shared_ptr<dao::MonitoringPeriodDao> periodDao_;
        std::shared_ptr<dao::TaskDao> taskDao_;
    };

}  // namespace soilwatercm::services
