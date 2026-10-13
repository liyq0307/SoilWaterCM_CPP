#include "monitoring_service.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <thread>

#include "../common/exceptions.h"
#include "../infrastructure/config_manager.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{

    namespace
    {
        // 对齐 Python services/monitoring_schedule.py 常量
        constexpr int kFloodSeasonMonths[] = {5, 6, 7, 8, 9};
        constexpr int kBaseIntervalDays = 30;
        constexpr int kFloodIntervalDays = 10;
        constexpr double kRainfallEmergencyMm = 50.0;
        constexpr double kCloudSnowMaxRatio = 0.10;  // settings.CLOUD_COVER_TOTAL_MAX

        bool isFloodSeason(int month)
        {
            return std::find(std::begin(kFloodSeasonMonths), std::end(kFloodSeasonMonths), month) !=
                   std::end(kFloodSeasonMonths);
        }

        int currentMonth()
        {
            auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm tm = {};
            localtime_s(&tm, &t);
            return tm.tm_mon + 1;
        }

        std::string tpToString(const std::chrono::system_clock::time_point& tp)
        {
            auto t = std::chrono::system_clock::to_time_t(tp);
            std::tm tm = {};
            localtime_s(&tm, &t);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
            return buf;
        }

        std::string fmtDouble(double v)
        {
            std::ostringstream oss;
            oss << v;
            return oss.str();
        }

        // 可选数值参数: 缺失/为 null 返回 nullopt
        std::optional<double> optNumber(const nlohmann::json& p, const std::string& key)
        {
            auto it = p.find(key);
            if (it == p.end() || it->is_null())
                return std::nullopt;
            if (it->is_number())
                return it->get<double>();
            if (it->is_string())
            {
                try
                {
                    return std::stod(it->get<std::string>());
                }
                catch (const std::exception&)
                {
                    return std::nullopt;
                }
            }
            return std::nullopt;
        }
    }  // namespace

    MonitoringService::MonitoringService(std::shared_ptr<dao::MonitoringPeriodDao> periodDao,
                                         std::shared_ptr<dao::TaskDao> taskDao)
        : periodDao_(std::move(periodDao)), taskDao_(std::move(taskDao))
    {
    }

    nlohmann::json MonitoringService::schedule(const nlohmann::json& params)
    {
        int month = params.value("month", 0);
        if (month < 1 || month > 12)
            month = currentMonth();

        auto rainfall = optNumber(params, "rainfall_mm");
        auto cloudSnow = optNumber(params, "cloud_snow_ratio");

        bool flood = isFloodSeason(month);
        int interval = flood ? kFloodIntervalDays : kBaseIntervalDays;
        bool emergency = rainfall.has_value() && *rainfall >= kRainfallEmergencyMm;
        bool substitute = cloudSnow.has_value() && *cloudSnow > kCloudSnowMaxRatio;

        nlohmann::json advice = nlohmann::json::array();
        if (flood)
            advice.push_back("汛期加密监测, 建议监测间隔缩短至 " + std::to_string(interval) + " 天");
        else
            advice.push_back("常规监测, 建议监测间隔 " + std::to_string(interval) + " 天");
        if (emergency)
            advice.push_back("强降雨(" + fmtDouble(*rainfall) +
                             "mm)超阈, 启动应急监测: 48h 内无人机详查重点区(边坡/堆渣场)");
        if (substitute)
            advice.push_back("云雪覆盖(" + fmtDouble(*cloudSnow * 100.0) + "%)超阈, 影像不可用, 启用多源/历史数据替代");

        nlohmann::json result;
        result["month"] = month;
        result["flood_season"] = flood;
        result["recommended_interval_days"] = interval;
        result["emergency_monitoring"] = emergency;
        result["rainfall_mm"] = rainfall.has_value() ? nlohmann::json(*rainfall) : nlohmann::json(nullptr);
        result["data_substitute_needed"] = substitute;
        result["cloud_snow_ratio"] = cloudSnow.has_value() ? nlohmann::json(*cloudSnow) : nlohmann::json(nullptr);
        result["advice"] = std::move(advice);

        // 扩展: 给定 project_id 时附带各期下次建议监测日期 (Python 无此逻辑)
        int projectId = params.value("project_id", 0);
        if (projectId > 0)
        {
            nlohmann::json periods = nlohmann::json::array();
            for (const auto& p : periodDao_->findByProjectId(projectId))
            {
                nlohmann::json item;
                item["period_id"] = p.id;
                item["period_code"] = p.period_code;
                item["end_date"] = tpToString(p.end_date);
                auto next = p.end_date + std::chrono::hours(24 * interval);
                item["next_due_date"] = tpToString(next);
                periods.push_back(std::move(item));
            }
            result["project_id"] = projectId;
            result["periods"] = std::move(periods);
        }

        return result;
    }

    nlohmann::json MonitoringService::rapidChange(int projectId, const nlohmann::json& params)
    {
        if (projectId <= 0)
            throw common::ValidationException("缺少 project_id");

        std::string mode = params.value("mode", "slope_collapse");
        if (mode != "slope_collapse" && mode != "dump_loss")
            throw common::ValidationException("未知 mode: " + mode + " (可选 slope_collapse / dump_loss)");

        std::string dsmBefore = params.value("dsm_before", "");
        std::string dsmAfter = params.value("dsm_after", "");
        if (dsmBefore.empty() || dsmAfter.empty())
            throw common::ValidationException("缺少 dsm_before / dsm_after");

        double dropThreshold = params.value("drop_threshold_m", 1.0);

        nlohmann::json taskParams;
        taskParams["task_type"] = "rapid_change";
        taskParams["mode"] = mode;
        taskParams["dsm_before"] = dsmBefore;
        taskParams["dsm_after"] = dsmAfter;
        taskParams["drop_threshold_m"] = dropThreshold;

        models::Task task;
        task.type = models::TaskType::Disturbance;  // 本项目 TaskType 无 rapid_change 枚举, 归入扰动并用 params.task_type 区分
        task.status = models::TaskStatus::Pending;
        task.progress = 0.0;
        task.params = taskParams.dump();
        task.project_id = projectId;
        task.period_id = params.value("period_id", 0);
        task.retry_count = 0;
        task.submitted_at = std::chrono::system_clock::now();

        int taskId = taskDao_->insert(task);
        if (taskId < 0)
            throw common::DatabaseException("创建快速变化检测任务失败");
        task.id = taskId;

        std::string outputDir = (std::filesystem::path(infrastructure::ConfigManager::instance().getStorageResultsDir()) /
                                 "dynamic" / ("p" + std::to_string(projectId)) / "rapid_change")
                                    .string();

        std::thread([this, taskId, outputDir]() { runRapidChangeTask(taskId, outputDir); }).detach();

        return taskToJson(task);
    }

    void MonitoringService::runRapidChangeTask(int taskId, std::string outputDir)
    {
        try
        {
            taskDao_->updateStatus(taskId, models::TaskStatus::Running, 10.0);
            std::filesystem::create_directories(outputDir);

            // DSM 高程骤降差值算法 (rasterio) 未移植: 写占位说明, 不产生异常记录。
            // 不再"假成功"置 Completed, 改显式 Failed 并写清原因。
            nlohmann::json placeholder;
            placeholder["status"] = "not_implemented";
            placeholder["note"] = "DSM 差值检测待移植, 未生成异常图斑";
            std::ofstream((std::filesystem::path(outputDir) / "rapid_change.json").string()) << placeholder.dump(2);

            taskDao_->updateResult(taskId, outputDir);
            taskDao_->updateStatusAndMessage(taskId, models::TaskStatus::Failed, 10.0,
                                             "rapid_change: DSM 差值检测未移植 (C++ 后端未实现), 未生成异常图斑");
            utils::Logger::warn("Monitoring: rapid_change task {} marked failed (not implemented)", taskId);
        }
        catch (const std::exception& e)
        {
            utils::Logger::error("Monitoring: rapid_change task {} failed: {}", taskId, e.what());
            try
            {
                taskDao_->updateStatus(taskId, models::TaskStatus::Failed, 0.0);
            }
            catch (...)
            {
            }
        }
    }

    nlohmann::json MonitoringService::taskToJson(const models::Task& task)
    {
        nlohmann::json j;
        j["id"] = task.id;
        j["type"] = static_cast<int>(task.type);
        j["status"] = static_cast<int>(task.status);
        j["progress"] = task.progress;
        if (!task.params.empty())
        {
            try
            {
                j["params"] = nlohmann::json::parse(task.params);
            }
            catch (const std::exception&)
            {
                j["params"] = task.params;
            }
        }
        else
        {
            j["params"] = nullptr;
        }
        j["result_path"] = task.result_path.value_or(std::string());
        j["error_message"] = task.error_message.value_or(std::string());
        j["submitted_at"] = tpToString(task.submitted_at);
        j["started_at"] = task.started_at ? nlohmann::json(tpToString(*task.started_at)) : nlohmann::json(nullptr);
        j["completed_at"] =
            task.completed_at ? nlohmann::json(tpToString(*task.completed_at)) : nlohmann::json(nullptr);
        j["retry_count"] = task.retry_count;
        j["project_id"] = task.project_id;
        j["period_id"] = task.period_id;
        return j;
    }

}  // namespace soilwatercm::services
