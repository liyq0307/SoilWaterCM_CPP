#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../dao/erosion_result_dao.h"
#include "../dao/monitoring_period_dao.h"
#include "../dao/project_dao.h"
#include "../dao/result_dao.h"
#include "../dao/terrain_result_dao.h"
#include "../dao/vegetation_coverage_dao.h"
#include "report_service.h"

namespace soilwatercm::services
{
    /**
     * @brief 定时任务调度器 (替代 Python Celery beat)
     *
     * 单线程 5 分钟粒度轮询, 按本地时钟窗口触发:
     * - 1/4/7/10 月 1 日 02 点: 为每个项目按最新监测期生成季度报告
     * - 1 月 1 日 03 点: 生成年度报告
     * - 每日 04 点: 清理 raster 文件已丢失的孤立成果记录
     * 每个任务当日仅触发一次 (进程内守卫), 服务停机期间错过的窗口不补跑。
     */
    class SchedulerService
    {
    public:
        SchedulerService(std::shared_ptr<dao::ProjectDao> projectDao,
                         std::shared_ptr<dao::MonitoringPeriodDao> periodDao,
                         std::shared_ptr<ReportService> reportService,
                         std::shared_ptr<dao::VegetationCoverageDao> fvcDao,
                         std::shared_ptr<dao::ErosionResultDao> erosionDao,
                         std::shared_ptr<dao::LandUseResultDao> landuseDao,
                         std::shared_ptr<dao::SegmentationResultDao> segmentationDao,
                         std::shared_ptr<dao::TerrainResultDao> terrainDao);

        ~SchedulerService() = default;

        /// @brief 启动调度线程 (幂等, 重复调用不重复起线程)
        void start();

    private:
        void loop();

        /// @brief 为每个项目按最新监测期出报; @param annual true=年报 false=季报
        void generateScheduledReports(bool annual);

        /// @brief 清理各成果表中 raster 文件全部丢失的孤立记录
        void cleanupOrphanResults();

        /// @brief 删除指定孤立 id 并计数; @param removeFn 按 id 删除 (各 DAO 的 remove)
        int deleteOrphans(const std::string& tableName, const std::vector<int>& orphanIds,
                         const std::function<bool(int)>& removeFn);

        std::shared_ptr<dao::ProjectDao> projectDao_;
        std::shared_ptr<dao::MonitoringPeriodDao> periodDao_;
        std::shared_ptr<ReportService> reportService_;
        std::shared_ptr<dao::VegetationCoverageDao> fvcDao_;
        std::shared_ptr<dao::ErosionResultDao> erosionDao_;
        std::shared_ptr<dao::LandUseResultDao> landuseDao_;
        std::shared_ptr<dao::SegmentationResultDao> segmentationDao_;
        std::shared_ptr<dao::TerrainResultDao> terrainDao_;

        std::thread thread_;
        std::mutex guardMutex_;
        std::string lastQuarterlyRun_;  ///< 已出报的季度报告日期 (YYYY-MM-DD)
        std::string lastAnnualRun_;     ///< 已出报的年度报告日期
        std::string lastCleanupRun_;    ///< 已执行孤立清理的日期
    };

}  // namespace soilwatercm::services
