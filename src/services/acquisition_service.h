#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "../dao/task_dao.h"
#include "../infrastructure/external/satellite_client.h"
#include "../models/task.h"
#include <nlohmann/json.hpp>

namespace soilwatercm::services
{

    /**
     * @brief 数据获取服务
     *
     * 同步检索卫星影像、异步采集下载卫星数据 (建任务 + 后台线程)、
     * 无人机航线规划 (纯几何计算) 与各数据源可用性诊断。
     */
    class AcquisitionService
    {
    public:
        /**
         * @brief 构造, 注入卫星客户端与任务 DAO
         * @param satelliteClient 卫星数据源客户端
         * @param taskDao 任务 DAO (采集任务记录)
         */
        AcquisitionService(std::shared_ptr<infrastructure::external::SatelliteClient> satelliteClient,
                           std::shared_ptr<dao::TaskDao> taskDao);

        ~AcquisitionService() = default;

        /**
         * @brief 同步检索可用卫星景
         * @param params 检索参数: source (sentinel/landsat/china)、bbox、
         *               start_date/end_date (YYYY-MM-DD)、cloud_cover (百分比, 默认 20)
         * @return {source, count, scenes}
         * @throw ValidationException 数据源未知 / bbox 非法 / 缺日期时
         * @throw ExternalToolException 卫星检索失败 (凭据问题等) 时
         */
        nlohmann::json searchSatellite(const nlohmann::json& params);

        /**
         * @brief 异步采集项目卫星数据
         *
         * 写 task 记录后由后台线程检索/下载, 本方法立即返回任务信息。
         * @param params 采集参数: project_id、bbox、start_date/end_date、
         *               data_types、cloud_cover 等
         * @return 任务信息 JSON (TaskRead 风格)
         * @throw ValidationException 缺 project_id 或缺日期时
         * @throw DatabaseException 创建采集任务失败时
         */
        nlohmann::json collectSatellite(const nlohmann::json& params);

        /**
         * @brief 无人机航线规划 (同步, 纯几何计算)
         * @param params bbox + GSD + 相机预设 (camera) + 重叠率等
         * @return 航高/像幅/航带数/蛇形航点列表
         * @throw ValidationException 未知相机预设或重叠率不在 [0,1) 时
         */
        nlohmann::json generateFlightPlan(const nlohmann::json& params);

        /**
         * @brief 各卫星数据源可用性诊断
         * @return 各数据源凭据/连通状态 JSON
         */
        nlohmann::json backendStatus();

        /**
         * @brief Task -> JSON (对齐 TaskRead 字段)
         * @param task 任务记录
         * @return JSON
         */
        static nlohmann::json taskToJson(const models::Task& task);

    private:
        std::shared_ptr<infrastructure::external::SatelliteClient> satelliteClient_;
        std::shared_ptr<dao::TaskDao> taskDao_;

        // 后台采集执行体 (独立线程): 检索各数据类型最新景并下载, 回写任务状态
        void runCollectTask(std::string bboxStr, std::string startDate, std::string endDate,
                            std::vector<std::string> dataTypes, double cloudCoverFrac, std::string outputDir,
                            int taskId);
    };

}  // namespace soilwatercm::services
