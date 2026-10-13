#include "scheduler_service.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <functional>
#include <sstream>
#include <vector>

#include "../models/project.h"
#include "../models/monitoring_period.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{
    namespace
    {
        std::string todayStr()
        {
            std::time_t t = std::time(nullptr);
            std::tm tmv = {};
#if defined(_WIN32)
            localtime_s(&tmv, &t);
#else
            localtime_r(&t, &tmv);
#endif
            char buf[16];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tmv);
            return buf;
        }

        std::string currentTimeStr()
        {
            std::time_t t = std::time(nullptr);
            std::tm tmv = {};
#if defined(_WIN32)
            localtime_s(&tmv, &t);
#else
            localtime_r(&t, &tmv);
#endif
            char buf[24];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
            return buf;
        }
    }  // namespace

    SchedulerService::SchedulerService(std::shared_ptr<dao::ProjectDao> projectDao,
                                       std::shared_ptr<dao::MonitoringPeriodDao> periodDao,
                                       std::shared_ptr<ReportService> reportService,
                                       std::shared_ptr<dao::VegetationCoverageDao> fvcDao,
                                       std::shared_ptr<dao::ErosionResultDao> erosionDao,
                                       std::shared_ptr<dao::LandUseResultDao> landuseDao,
                                       std::shared_ptr<dao::SegmentationResultDao> segmentationDao,
                                       std::shared_ptr<dao::TerrainResultDao> terrainDao)
        : projectDao_(std::move(projectDao)), periodDao_(std::move(periodDao)),
          reportService_(std::move(reportService)), fvcDao_(std::move(fvcDao)),
          erosionDao_(std::move(erosionDao)), landuseDao_(std::move(landuseDao)),
          segmentationDao_(std::move(segmentationDao)), terrainDao_(std::move(terrainDao))
    {
    }

    void SchedulerService::start()
    {
        std::lock_guard<std::mutex> lock(guardMutex_);
        if (thread_.joinable())
            return;
        thread_ = std::thread([this]() { loop(); });
        utils::Logger::info("Scheduler started (5-min tick, local time windows)");
    }

    void SchedulerService::loop()
    {
        while (true)
        {
            std::time_t nowT = std::time(nullptr);
            std::tm tmv = {};
#if defined(_WIN32)
            localtime_s(&tmv, &nowT);
#else
            localtime_r(&nowT, &tmv);
#endif
            int hour = tmv.tm_hour;
            int day = tmv.tm_mday;
            int month = tmv.tm_mon + 1;

            // 季度报告: 1/4/7/10 月 1 日 02 点窗口
            bool isQuarterMonth = (month == 1 || month == 4 || month == 7 || month == 10);
            if (isQuarterMonth && day == 1 && hour == 2 && lastQuarterlyRun_ != todayStr())
            {
                generateScheduledReports(false);
                lastQuarterlyRun_ = todayStr();
            }

            // 年度报告: 1 月 1 日 03 点窗口
            if (month == 1 && day == 1 && hour == 3 && lastAnnualRun_ != todayStr())
            {
                generateScheduledReports(true);
                lastAnnualRun_ = todayStr();
            }

            // 孤立成果清理: 每日 04 点窗口
            if (hour == 4 && lastCleanupRun_ != todayStr())
            {
                cleanupOrphanResults();
                lastCleanupRun_ = todayStr();
            }

            std::this_thread::sleep_for(std::chrono::minutes(5));
        }
    }

    void SchedulerService::generateScheduledReports(bool annual)
    {
        utils::Logger::info("Scheduler: generating {} scheduled reports", annual ? "annual" : "quarterly");
        auto projects = projectDao_->list(1, 10000, "id", false);
        int generated = 0;
        for (const auto& p : projects)
        {
            try
            {
                // 取该项目最新监测期 (按开始日期倒序), 对齐 Python task_scheduled_reports
                auto periods = periodDao_->findByProjectId(p.id);
                if (periods.empty())
                    continue;
                auto latest = *std::max_element(periods.begin(), periods.end(),
                                                [](const models::MonitoringPeriod& a, const models::MonitoringPeriod& b)
                                                { return a.start_date < b.start_date; });

                ReportGenerateParams params;
                params.project_id = p.id;
                params.period_ids = {latest.id};
                params.type = annual ? models::ReportType::Annual : models::ReportType::Quarterly;
                params.title = std::string(annual ? "年度水土保持监测报告" : "季度水土保持监测报告") + " - " + p.name +
                               " - " + currentTimeStr();
                reportService_->generateReport(params);
                ++generated;
            }
            catch (const std::exception& e)
            {
                utils::Logger::error("Scheduler: report for project {} failed: {}", p.id, e.what());
            }
        }
        utils::Logger::info("Scheduler: {} report(s) generated for {} project(s)", generated, projects.size());
    }

    void SchedulerService::cleanupOrphanResults()
    {
        utils::Logger::info("Scheduler: cleaning orphan result records");

        int total = 0;

        // vegetation_coverages: 单路径字段 fvc_raster_path
        {
            std::vector<int> orphan;
            for (const auto& v : fvcDao_->list(1, 100000, "id", false))
                if (!v.fvc_raster_path.empty() && !std::filesystem::exists(v.fvc_raster_path))
                    orphan.push_back(v.id);
            total += deleteOrphans("vegetation_coverages", orphan,
                                   [this](int id) { return fvcDao_->remove(id); });
        }

        // erosion_results: modulus_raster_path + class_raster_path (至少一个非空且全丢失才算孤立)
        {
            std::vector<int> orphan;
            for (const auto& e : erosionDao_->list(1, 100000, "id", false))
            {
                bool hasPath = false;
                bool allMissing = true;
                for (const auto& p : {e.modulus_raster_path, e.class_raster_path})
                {
                    if (p.empty())
                        continue;
                    hasPath = true;
                    if (std::filesystem::exists(p))
                        allMissing = false;
                }
                if (hasPath && allMissing)
                    orphan.push_back(e.id);
            }
            total += deleteOrphans("erosion_results", orphan,
                                   [this](int id) { return erosionDao_->remove(id); });
        }

        // landuse_results / segmentation_results / terrain_results: 单路径字段 raster_path
        {
            std::vector<int> orphan;
            for (const auto& l : landuseDao_->list(1, 100000, "id", false))
                if (!l.raster_path.empty() && !std::filesystem::exists(l.raster_path))
                    orphan.push_back(l.id);
            total += deleteOrphans("landuse_results", orphan,
                                   [this](int id) { return landuseDao_->remove(id); });
        }
        {
            std::vector<int> orphan;
            for (const auto& s : segmentationDao_->list(1, 100000, "id", false))
                if (!s.raster_path.empty() && !std::filesystem::exists(s.raster_path))
                    orphan.push_back(s.id);
            total += deleteOrphans("segmentation_results", orphan,
                                   [this](int id) { return segmentationDao_->remove(id); });
        }
        {
            std::vector<int> orphan;
            for (const auto& t : terrainDao_->list(1, 100000, "id", false))
                if (!t.raster_path.empty() && !std::filesystem::exists(t.raster_path))
                    orphan.push_back(t.id);
            total += deleteOrphans("terrain_results", orphan,
                                   [this](int id) { return terrainDao_->remove(id); });
        }

        utils::Logger::info("Scheduler: orphan cleanup finished, deleted={}", total);
    }

    int SchedulerService::deleteOrphans(const std::string& tableName, const std::vector<int>& orphanIds,
                                        const std::function<bool(int)>& removeFn)
    {
        int deleted = 0;
        for (int id : orphanIds)
        {
            if (removeFn(id))
                ++deleted;
        }
        if (deleted > 0)
            utils::Logger::info("Scheduler: cleaned {} orphan record(s) from {}", deleted, tableName);
        return deleted;
    }

}  // namespace soilwatercm::services
