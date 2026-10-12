#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "../dao/anomaly_dao.h"
#include "../dao/disturbance_dao.h"
#include "../dao/erosion_result_dao.h"
#include "../dao/image_dao.h"
#include "../dao/indicator_dao.h"
#include "../dao/measure_dao.h"
#include "../dao/monitoring_period_dao.h"
#include "../dao/project_dao.h"
#include "../dao/task_dao.h"
#include "../dao/vegetation_coverage_dao.h"

namespace soilwatercm::services
{

    /** @brief 全局看板汇总数据 */
    struct DashboardData
    {
        int total_projects = 0;      ///< 项目总数
        int active_projects = 0;     ///< 进行中项目数
        int total_images = 0;        ///< 影像总数
        int pending_tasks = 0;       ///< 待处理任务数
        int running_tasks = 0;       ///< 运行中任务数
        int completed_tasks = 0;     ///< 已完成任务数
        int failed_tasks = 0;        ///< 失败任务数
        int pending_anomalies = 0;   ///< 待处理异常数
        std::map<std::string, int> task_statistics;       ///< 任务分类统计
        std::map<std::string, double> area_statistics;    ///< 面积统计
    };

    /** @brief 个人工作台数据 */
    struct WorkbenchData
    {
        std::vector<std::map<std::string, std::string>> recent_projects;      ///< 最近项目
        std::vector<std::map<std::string, std::string>> recent_tasks;         ///< 最近任务
        std::vector<std::map<std::string, std::string>> pending_anomalies;    ///< 待处理异常
        std::map<std::string, int> quick_stats;                               ///< 快捷计数
    };

    /**
     * @brief 统计看板服务
     *
     * 全局概览与个人工作台聚合, 以及项目维度的汇总/趋势/分期指标。
     * 扩展 DAO 未注入时按全局配置惰性自建, 避免改动装配代码。
     */
    class DashboardService
    {
    public:
        /**
         * @brief 构造, 注入项目/影像/任务/异常 DAO
         * @param projectDao 项目 DAO
         * @param imageDao 影像 DAO
         * @param taskDao 任务 DAO
         * @param anomalyDao 异常 DAO
         */
        explicit DashboardService(std::shared_ptr<dao::ProjectDao> projectDao, std::shared_ptr<dao::ImageDao> imageDao,
                                  std::shared_ptr<dao::TaskDao> taskDao, std::shared_ptr<dao::AnomalyDao> anomalyDao);

        ~DashboardService() = default;

        /**
         * @brief 全局汇总
         * @return 项目/影像/任务/异常计数 + 任务与面积统计
         */
        DashboardData getDashboardData();

        /**
         * @brief 工作台数据
         * @param userId 用户 id
         * @return 最近 5 个项目/任务、待处理异常与快捷计数
         */
        WorkbenchData getWorkbenchData(int userId);

        /**
         * @brief 时序动画; 当前为占位实现, 仅返回传入的输出路径
         * @param projectId 项目 id
         * @param periodIds 期次 id 列表
         * @param indicator 指标编码
         * @param outputPath 输出路径
         * @return 输出路径
         */
        std::string generateTimeSeriesAnimation(int projectId, const std::vector<int>& periodIds,
                                                const std::string& indicator, const std::string& outputPath);

        /**
         * @brief 按状态统计任务数
         * @return {pending, running, completed, failed}
         */
        std::map<std::string, int> getTaskStatistics();

        /**
         * @brief 面积统计; 当前为占位实现, 按项目数估算而非真实几何面积
         * @return 各分区/类别面积
         */
        std::map<std::string, double> getAreaStatistics();

        // ---- 项目维度看板 (对齐 Python dashboard_service.py build_*) ----

        /**
         * @brief 汇总统计 (DashboardSummary 字段)
         * @param projectId 项目 id
         * @return 汇总 JSON; 项目不存在时返回 {project_id, project_name:"未知项目"}
         */
        nlohmann::json buildSummary(int projectId);

        /**
         * @brief 单指标跨期趋势 (TrendPoint 列表)
         * @param projectId 项目 id
         * @param indicatorCode 指标编码
         * @return 趋势 JSON; 指标不存在返回空数组
         */
        nlohmann::json buildTrend(int projectId, const std::string& indicatorCode);

        /**
         * @brief 多指标跨期趋势 (MultiTrendSeries 列表)
         * @param projectId 项目 id
         * @return 趋势 JSON
         */
        nlohmann::json buildMultiTrend(int projectId);

        /**
         * @brief 单期全指标值 (PeriodIndicators)
         * @param projectId 项目 id
         * @param periodId 期次 id
         * @return 指标值 JSON
         */
        nlohmann::json buildPeriodIndicators(int projectId, int periodId);

    private:
        std::shared_ptr<dao::ProjectDao> projectDao_;
        std::shared_ptr<dao::ImageDao> imageDao_;
        std::shared_ptr<dao::TaskDao> taskDao_;
        std::shared_ptr<dao::AnomalyDao> anomalyDao_;

        // main.cpp 未注入的 DAO: 按全局配置惰性构建 (进程内共享一个连接池, 同 ProjectService 模式)
        std::shared_ptr<dao::MonitoringPeriodDao> periodDao();
        std::shared_ptr<dao::IndicatorDao> indicatorDao();
        std::shared_ptr<dao::DisturbanceDao> disturbanceDao();
        std::shared_ptr<dao::VegetationCoverageDao> fvcDao();
        std::shared_ptr<dao::ErosionResultDao> erosionDao();
        std::shared_ptr<dao::MeasureDao> measureDao();

        std::shared_ptr<repository::PostgresRepository> sharedRepo();
        std::shared_ptr<dao::MonitoringPeriodDao> periodDao_;
        std::shared_ptr<dao::IndicatorDao> indicatorDao_;
        std::shared_ptr<dao::DisturbanceDao> disturbanceDao_;
        std::shared_ptr<dao::VegetationCoverageDao> fvcDao_;
        std::shared_ptr<dao::ErosionResultDao> erosionDao_;
        std::shared_ptr<dao::MeasureDao> measureDao_;
    };

}  // namespace soilwatercm::services
