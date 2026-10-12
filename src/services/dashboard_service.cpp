#include "dashboard_service.h"

#include <algorithm>
#include <filesystem>
#include <set>

#include "../infrastructure/config_manager.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{

    DashboardService::DashboardService(std::shared_ptr<dao::ProjectDao> projectDao,
                                       std::shared_ptr<dao::ImageDao> imageDao, std::shared_ptr<dao::TaskDao> taskDao,
                                       std::shared_ptr<dao::AnomalyDao> anomalyDao)
        : projectDao_(std::move(projectDao)),
          imageDao_(std::move(imageDao)),
          taskDao_(std::move(taskDao)),
          anomalyDao_(std::move(anomalyDao))
    {
    }

    DashboardData DashboardService::getDashboardData()
    {
        DashboardData data;

        auto projects = projectDao_->list(1, 1000);
        data.total_projects = static_cast<int>(projects.size());

        for (const auto& project : projects)
        {
            if (project.status == models::ProjectStatus::Active)
            {
                data.active_projects++;
            }
        }

        auto images = imageDao_->list(1, 1000);
        data.total_images = static_cast<int>(images.size());

        auto pendingTasks = taskDao_->findByStatus(models::TaskStatus::Pending);
        data.pending_tasks = static_cast<int>(pendingTasks.size());

        auto runningTasks = taskDao_->findByStatus(models::TaskStatus::Running);
        data.running_tasks = static_cast<int>(runningTasks.size());

        auto completedTasks = taskDao_->findByStatus(models::TaskStatus::Completed);
        data.completed_tasks = static_cast<int>(completedTasks.size());

        auto failedTasks = taskDao_->findByStatus(models::TaskStatus::Failed);
        data.failed_tasks = static_cast<int>(failedTasks.size());

        auto pendingAnomalies = anomalyDao_->findByStatus(models::DisposalStatus::Pending);
        data.pending_anomalies = static_cast<int>(pendingAnomalies.size());

        data.task_statistics = getTaskStatistics();
        data.area_statistics = getAreaStatistics();

        return data;
    }

    WorkbenchData DashboardService::getWorkbenchData(int userId)
    {
        WorkbenchData data;

        auto recentProjects = projectDao_->list(1, 5);
        for (const auto& project : recentProjects)
        {
            std::map<std::string, std::string> item;
            item["id"] = std::to_string(project.id);
            item["code"] = project.code;
            item["name"] = project.name;
            data.recent_projects.push_back(item);
        }

        auto recentTasks = taskDao_->listRecent(5);
        for (const auto& task : recentTasks)
        {
            std::map<std::string, std::string> item;
            item["id"] = std::to_string(task.id);
            item["type"] = std::to_string(static_cast<int>(task.type));
            item["status"] = std::to_string(static_cast<int>(task.status));
            item["progress"] = std::to_string(task.progress);
            data.recent_tasks.push_back(item);
        }

        auto pendingAnomalies = anomalyDao_->findByStatus(models::DisposalStatus::Pending);
        for (const auto& anomaly : pendingAnomalies)
        {
            std::map<std::string, std::string> item;
            item["id"] = std::to_string(anomaly.id);
            item["type"] = std::to_string(static_cast<int>(anomaly.type));
            item["description"] = anomaly.description;
            data.pending_anomalies.push_back(item);
        }

        data.quick_stats["total_projects"] = projectDao_->count();
        data.quick_stats["total_images"] = imageDao_->count();
        data.quick_stats["pending_tasks"] =
            static_cast<int>(taskDao_->findByStatus(models::TaskStatus::Pending).size());

        return data;
    }

    std::string DashboardService::generateTimeSeriesAnimation(int projectId, const std::vector<int>& periodIds,
                                                              const std::string& indicator,
                                                              const std::string& outputPath)
    {
        utils::Logger::info("Generating time series animation: project={}, indicator={}", projectId, indicator);

        return outputPath;
    }

    std::map<std::string, int> DashboardService::getTaskStatistics()
    {
        std::map<std::string, int> stats;

        stats["pending"] = static_cast<int>(taskDao_->findByStatus(models::TaskStatus::Pending).size());
        stats["running"] = static_cast<int>(taskDao_->findByStatus(models::TaskStatus::Running).size());
        stats["completed"] = static_cast<int>(taskDao_->findByStatus(models::TaskStatus::Completed).size());
        stats["failed"] = static_cast<int>(taskDao_->findByStatus(models::TaskStatus::Failed).size());

        return stats;
    }

    std::map<std::string, double> DashboardService::getAreaStatistics()
    {
        std::map<std::string, double> stats;

        auto projects = projectDao_->list(1, 1000);

        double totalArea = 0.0;
        for (const auto& project : projects)
        {
            if (!project.responsibility_area.wkt.empty())
            {
                totalArea += 100.0;
            }
        }

        stats["total_area_ha"] = totalArea;
        stats["active_area_ha"] = totalArea * 0.8;

        return stats;
    }

    namespace
    {
        std::string tpToIso(const std::chrono::system_clock::time_point& tp)
        {
            auto t = std::chrono::system_clock::to_time_t(tp);
            std::tm tm = {};
            localtime_s(&tm, &t);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
            return buf;
        }

        // 文件型指标编码 (对齐 Python FILE_BACKED_INDICATOR_MODELS)
        bool isFileBackedIndicator(const std::string& code)
        {
            return code == "fvc_mean" || code == "erosion_modulus" || code == "erosion_high_ratio";
        }
    }  // namespace

    std::shared_ptr<repository::PostgresRepository> DashboardService::sharedRepo()
    {
        // main.cpp 未注入扩展 DAO 时, 按全局配置惰性构建 (进程内共享一个连接池, 同 ProjectService 模式)
        static std::shared_ptr<repository::PostgresRepository> repo = []()
        {
            auto& config = infrastructure::ConfigManager::getInstance();
            repository::PostgresConfig pgCfg;
            pgCfg.host = config.getPostgresHost();
            pgCfg.port = config.getPostgresPort();
            pgCfg.database = config.getPostgresDatabase();
            pgCfg.user = config.getPostgresUser();
            pgCfg.password = config.getPostgresPassword();
            auto pool = std::make_shared<repository::PostgresConnectionPool>(pgCfg);
            return std::make_shared<repository::PostgresRepository>(pool);
        }();
        return repo;
    }

    std::shared_ptr<dao::MonitoringPeriodDao> DashboardService::periodDao()
    {
        if (!periodDao_)
            periodDao_ = std::make_shared<dao::MonitoringPeriodDao>(sharedRepo());
        return periodDao_;
    }

    std::shared_ptr<dao::IndicatorDao> DashboardService::indicatorDao()
    {
        if (!indicatorDao_)
            indicatorDao_ = std::make_shared<dao::IndicatorDao>(sharedRepo());
        return indicatorDao_;
    }

    std::shared_ptr<dao::DisturbanceDao> DashboardService::disturbanceDao()
    {
        if (!disturbanceDao_)
            disturbanceDao_ = std::make_shared<dao::DisturbanceDao>(sharedRepo());
        return disturbanceDao_;
    }

    std::shared_ptr<dao::VegetationCoverageDao> DashboardService::fvcDao()
    {
        if (!fvcDao_)
            fvcDao_ = std::make_shared<dao::VegetationCoverageDao>(sharedRepo());
        return fvcDao_;
    }

    std::shared_ptr<dao::ErosionResultDao> DashboardService::erosionDao()
    {
        if (!erosionDao_)
            erosionDao_ = std::make_shared<dao::ErosionResultDao>(sharedRepo());
        return erosionDao_;
    }

    std::shared_ptr<dao::MeasureDao> DashboardService::measureDao()
    {
        if (!measureDao_)
            measureDao_ = std::make_shared<dao::MeasureDao>(sharedRepo());
        return measureDao_;
    }

    nlohmann::json DashboardService::buildSummary(int projectId)
    {
        nlohmann::json result;
        result["project_id"] = projectId;

        auto projectOpt = projectDao_->findById(projectId);
        if (!projectOpt)
        {
            // 对齐 Python DashboardSummary 默认值
            result["project_name"] = "未知项目";
            result["current_period"] = nullptr;
            result["total_disturbance_ha"] = 0.0;
            result["new_disturbance_ha"] = 0.0;
            result["fvc_mean"] = nullptr;
            result["avg_erosion_modulus"] = nullptr;
            result["active_anomalies"] = 0;
            result["critical_anomalies"] = 0;
            result["measure_implementation_rate"] = nullptr;
            result["compliance_score"] = nullptr;
            result["risk_level"] = "unknown";
            result["next_period_due"] = nullptr;
            return result;
        }
        result["project_name"] = projectOpt->name;

        // 最新一期 (按 end_date 降序)
        auto periods = periodDao()->findByProjectId(projectId);
        std::sort(periods.begin(), periods.end(),
                  [](const models::MonitoringPeriod& a, const models::MonitoringPeriod& b)
                  { return a.end_date > b.end_date; });
        const models::MonitoringPeriod* current = periods.empty() ? nullptr : &periods.front();

        result["current_period"] = current ? nlohmann::json(current->period_code) : nlohmann::json(nullptr);

        // 扰动总面积 (C++ 模型无 extract_method 字段, 真增量以当期图斑面积近似)
        auto disturbances = disturbanceDao()->findByProject(projectId);
        double totalDisturbM2 = 0.0;
        double newDisturbM2 = 0.0;
        for (const auto& d : disturbances)
        {
            totalDisturbM2 += d.area;
            if (current && d.period_id == current->id)
                newDisturbM2 += d.area;
        }
        result["total_disturbance_ha"] = totalDisturbM2 / 10000.0;
        result["new_disturbance_ha"] = newDisturbM2 / 10000.0;

        std::optional<double> fvcMean;
        std::optional<double> erosionModulus;
        if (current)
        {
            auto veg = fvcDao()->findByPeriodId(current->id);
            if (veg)
                fvcMean = veg->mean_fvc;
            auto ero = erosionDao()->findByPeriodId(current->id);
            if (ero)
                erosionModulus = ero->mean_modulus;
        }
        result["fvc_mean"] = fvcMean.has_value() ? nlohmann::json(*fvcMean) : nlohmann::json(nullptr);
        result["avg_erosion_modulus"] =
            erosionModulus.has_value() ? nlohmann::json(*erosionModulus) : nlohmann::json(nullptr);

        // 异常统计: 本项目 DisposalStatus 无 resolved 细分, 以 status != Completed 计活跃/未解决
        int active = 0;
        int critical = 0;
        for (const auto& a : anomalyDao_->findByProjectId(projectId))
        {
            if (a.status != models::DisposalStatus::Completed)
            {
                active++;
                if (a.severity == models::Severity::Critical)
                    critical++;
            }
        }
        result["active_anomalies"] = active;
        result["critical_anomalies"] = critical;

        // C++ Measure 模型无 implementation_rate 字段, 置 null (与 Python 偏差)
        result["measure_implementation_rate"] = nullptr;
        result["compliance_score"] = nullptr;

        std::string risk = "low";
        if (critical > 0)
            risk = "high";
        else if (active > 5)
            risk = "medium";
        result["risk_level"] = risk;

        result["next_period_due"] =
            current ? nlohmann::json(tpToIso(current->end_date + std::chrono::hours(24 * 30))) : nlohmann::json(nullptr);

        return result;
    }

    nlohmann::json DashboardService::buildTrend(int projectId, const std::string& indicatorCode)
    {
        nlohmann::json points = nlohmann::json::array();

        auto ind = indicatorDao()->findByCode(indicatorCode);
        if (!ind)
            return points;

        auto values = indicatorDao()->listValues(projectId, std::nullopt, ind->id);

        // 期次索引
        std::map<int, models::MonitoringPeriod> periodById;
        for (const auto& p : periodDao()->findByProjectId(projectId))
            periodById[p.id] = p;

        // 文件型指标: 该期成果记录仍在才保留 (物理文件校验简化为 DB 记录存在性)
        std::set<int> backedPeriods;
        if (isFileBackedIndicator(indicatorCode))
        {
            for (const auto& [pid, _] : periodById)
            {
                bool backed = (indicatorCode == "fvc_mean") ? fvcDao()->findByPeriodId(pid).has_value()
                                                            : erosionDao()->findByPeriodId(pid).has_value();
                if (backed)
                    backedPeriods.insert(pid);
            }
        }

        struct Row
        {
            std::chrono::system_clock::time_point end_date;
            std::string period_code;
            double value;
            std::string label;
        };
        std::vector<Row> rows;
        for (const auto& v : values)
        {
            if (isFileBackedIndicator(indicatorCode) && !backedPeriods.count(v.period_id))
                continue;
            auto it = periodById.find(v.period_id);
            if (it == periodById.end())
                continue;
            rows.push_back({it->second.end_date, it->second.period_code, v.raw_value, v.level});
        }
        std::sort(rows.begin(), rows.end(),
                  [](const Row& a, const Row& b) { return a.end_date < b.end_date; });

        for (const auto& r : rows)
        {
            nlohmann::json pt;
            pt["period_code"] = r.period_code;
            pt["period_date"] = tpToIso(r.end_date);
            pt["value"] = r.value;
            pt["label"] = r.label.empty() ? nlohmann::json(nullptr) : nlohmann::json(r.label);
            points.push_back(std::move(pt));
        }
        return points;
    }

    nlohmann::json DashboardService::buildMultiTrend(int projectId)
    {
        nlohmann::json series = nlohmann::json::array();

        auto indicators = indicatorDao()->listActive("");
        std::map<int, const models::Indicator*> indById;
        for (const auto& ind : indicators)
            indById[ind.id] = &ind;

        std::map<int, models::MonitoringPeriod> periodById;
        for (const auto& p : periodDao()->findByProjectId(projectId))
            periodById[p.id] = p;

        auto values = indicatorDao()->listValues(projectId, std::nullopt, std::nullopt);

        // 按 (dimension, code) 排序后聚合 (对齐 Python order_by dimension, code, end_date)
        struct Flat
        {
            std::string dimension;
            std::string code;
            std::string name;
            std::string unit;
            std::chrono::system_clock::time_point end_date;
            std::string period_code;
            double value;
            std::string label;
        };
        std::vector<Flat> flat;
        for (const auto& v : values)
        {
            auto iit = indById.find(v.indicator_id);
            auto pit = periodById.find(v.period_id);
            if (iit == indById.end() || pit == periodById.end())
                continue;
            const auto& ind = *iit->second;
            // 文件型指标成果缺失的期点过滤 (同 buildTrend 简化策略)
            if (isFileBackedIndicator(ind.code))
            {
                bool backed = (ind.code == "fvc_mean") ? fvcDao()->findByPeriodId(v.period_id).has_value()
                                                       : erosionDao()->findByPeriodId(v.period_id).has_value();
                if (!backed)
                    continue;
            }
            flat.push_back({ind.dimension, ind.code, ind.name, ind.unit.value_or(""), pit->second.end_date,
                           pit->second.period_code, v.raw_value, v.level});
        }
        std::sort(flat.begin(), flat.end(),
                  [](const Flat& a, const Flat& b)
                  {
                      if (a.dimension != b.dimension)
                          return a.dimension < b.dimension;
                      if (a.code != b.code)
                          return a.code < b.code;
                      return a.end_date < b.end_date;
                  });

        // assemble_multi_trend: 按指标分组保序
        std::vector<std::string> order;
        std::map<std::string, nlohmann::json> grouped;
        for (const auto& r : flat)
        {
            if (!grouped.count(r.code))
            {
                nlohmann::json s;
                s["dimension"] = r.dimension;
                s["indicator_code"] = r.code;
                s["indicator_name"] = r.name;
                s["unit"] = r.unit.empty() ? nlohmann::json(nullptr) : nlohmann::json(r.unit);
                s["points"] = nlohmann::json::array();
                grouped[r.code] = std::move(s);
                order.push_back(r.code);
            }
            nlohmann::json pt;
            pt["period_code"] = r.period_code;
            pt["period_date"] = tpToIso(r.end_date);
            pt["value"] = r.value;
            pt["label"] = r.label.empty() ? nlohmann::json(nullptr) : nlohmann::json(r.label);
            grouped[r.code]["points"].push_back(std::move(pt));
        }
        for (const auto& code : order)
            series.push_back(grouped[code]);

        return series;
    }

    nlohmann::json DashboardService::buildPeriodIndicators(int projectId, int periodId)
    {
        auto period = periodDao()->findById(periodId);

        auto indicators = indicatorDao()->listActive("");
        std::map<int, const models::Indicator*> indById;
        for (const auto& ind : indicators)
            indById[ind.id] = &ind;

        auto values = indicatorDao()->listValues(projectId, periodId, std::nullopt);

        struct Item
        {
            std::string dimension;
            std::string code;
            std::string name;
            std::string unit;
            double value;
            std::string level;
        };
        std::vector<Item> items;
        for (const auto& v : values)
        {
            auto it = indById.find(v.indicator_id);
            if (it == indById.end())
                continue;
            const auto& ind = *it->second;
            if (isFileBackedIndicator(ind.code))
            {
                bool backed = (ind.code == "fvc_mean") ? fvcDao()->findByPeriodId(periodId).has_value()
                                                       : erosionDao()->findByPeriodId(periodId).has_value();
                if (!backed)
                    continue;
            }
            items.push_back({ind.dimension, ind.code, ind.name, ind.unit.value_or(""), v.raw_value, v.level});
        }
        std::sort(items.begin(), items.end(),
                  [](const Item& a, const Item& b)
                  {
                      if (a.dimension != b.dimension)
                          return a.dimension < b.dimension;
                      return a.code < b.code;
                  });

        nlohmann::json arr = nlohmann::json::array();
        for (const auto& r : items)
        {
            nlohmann::json j;
            j["indicator_code"] = r.code;
            j["indicator_name"] = r.name;
            j["dimension"] = r.dimension;
            j["unit"] = r.unit.empty() ? nlohmann::json(nullptr) : nlohmann::json(r.unit);
            j["value"] = r.value;
            j["level"] = r.level.empty() ? nlohmann::json(nullptr) : nlohmann::json(r.level);
            arr.push_back(std::move(j));
        }

        nlohmann::json result;
        result["period_id"] = periodId;
        result["period_code"] = period ? nlohmann::json(period->period_code) : nlohmann::json(nullptr);
        result["indicators"] = std::move(arr);
        return result;
    }

}  // namespace soilwatercm::services
