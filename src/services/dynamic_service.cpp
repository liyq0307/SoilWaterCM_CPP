#include "dynamic_service.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <thread>

#include "../common/exceptions.h"
#include "../infrastructure/config_manager.h"
#include "../models/monitoring_period.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{

    namespace
    {
        // 对齐 Python dynamic.py VALID_LAYERS
        const std::set<std::string>& validLayers()
        {
            static const std::set<std::string> layers = {
                "fvc", "coverage", "erosion", "landuse", "disturbance", "measure",
            };
            return layers;
        }

        std::string toLowerStr(const std::string& s)
        {
            std::string r = s;
            std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return std::tolower(c); });
            return r;
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

        std::string tpToDate(const std::chrono::system_clock::time_point& tp)
        {
            auto t = std::chrono::system_clock::to_time_t(tp);
            std::tm tm = {};
            localtime_s(&tm, &t);
            char buf[16];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
            return buf;
        }

        std::string joinComma(const std::set<std::string>& s)
        {
            std::string out;
            for (const auto& v : s)
                out += (out.empty() ? "" : ", ") + v;
            return out;
        }

        bool taskTypeMatches(const models::Task& t, const std::string& taskType)
        {
            if (t.params.empty())
                return false;
            try
            {
                auto j = nlohmann::json::parse(t.params);
                return j.value("task_type", "") == taskType;
            }
            catch (const std::exception&)
            {
                return false;
            }
        }
    }  // namespace

    DynamicService::DynamicService(std::shared_ptr<dao::MonitoringPeriodDao> periodDao,
                                   std::shared_ptr<dao::ImageDao> imageDao, std::shared_ptr<dao::TaskDao> taskDao,
                                   std::shared_ptr<dao::IndicatorDao> indicatorDao,
                                   std::shared_ptr<dao::VegetationCoverageDao> fvcDao,
                                   std::shared_ptr<dao::ErosionResultDao> erosionDao,
                                   std::shared_ptr<dao::DisturbanceDao> disturbanceDao,
                                   std::shared_ptr<dao::MeasureDao> measureDao)
        : periodDao_(std::move(periodDao)), imageDao_(std::move(imageDao)), taskDao_(std::move(taskDao)),
          indicatorDao_(std::move(indicatorDao)), fvcDao_(std::move(fvcDao)), erosionDao_(std::move(erosionDao)),
          disturbanceDao_(std::move(disturbanceDao)), measureDao_(std::move(measureDao))
    {
    }

    std::string DynamicService::dynamicRoot()
    {
        return (std::filesystem::path(infrastructure::ConfigManager::instance().getStorageResultsDir()) / "dynamic")
            .string();
    }

    void DynamicService::checkLayer(const std::string& layer)
    {
        if (!validLayers().count(layer))
            throw common::ValidationException("未知图层 " + layer + ", 可选: " + joinComma(validLayers()));
    }

    bool DynamicService::periodLayerSourceExists(int projectId, int periodId, const std::string& layer)
    {
        (void)projectId;
        if (layer == "fvc" || layer == "coverage")
            return fvcDao_->findByPeriodId(periodId).has_value();
        if (layer == "erosion")
            return erosionDao_->findByPeriodId(periodId).has_value();
        if (layer == "disturbance")
            return !disturbanceDao_->findByPeriodId(periodId).empty();
        if (layer == "measure")
            return !measureDao_->findByPeriodId(periodId).empty();
        // landuse: 本项目无土地利用成果表
        return false;
    }

    int DynamicService::createAsyncTask(const std::string& taskType, int projectId, const nlohmann::json& taskParams)
    {
        models::Task task;
        task.type = models::TaskType::Preprocess;  // 本项目 TaskType 无渲染枚举, 归入预处理并用 params.task_type 区分
        task.status = models::TaskStatus::Pending;
        task.progress = 0.0;
        nlohmann::json p = taskParams;
        p["task_type"] = taskType;
        task.params = p.dump();
        task.project_id = projectId;
        task.period_id = 0;
        task.retry_count = 0;
        task.submitted_at = std::chrono::system_clock::now();

        int taskId = taskDao_->insert(task);
        if (taskId < 0)
            throw common::DatabaseException("创建任务失败: " + taskType);
        return taskId;
    }

    nlohmann::json DynamicService::periodLayers(int projectId, const std::string& layer)
    {
        checkLayer(layer);
        if (projectId <= 0)
            throw common::ValidationException("缺少 project_id");

        auto periods = periodDao_->findByProjectId(projectId);
        std::sort(periods.begin(), periods.end(),
                  [](const models::MonitoringPeriod& a, const models::MonitoringPeriod& b)
                  { return a.end_date < b.end_date; });

        // 缓存检查: PNG 渲染未移植, 以 "专题数据源存在" 视为该期图层可用
        nlohmann::json items = nlohmann::json::array();
        nlohmann::json missing = nlohmann::json::array();
        for (const auto& p : periods)
        {
            if (periodLayerSourceExists(projectId, p.id, layer))
            {
                nlohmann::json item;
                item["period_id"] = p.id;
                item["period_code"] = p.period_code;
                item["period_date"] = tpToDate(p.end_date);
                item["image_count"] = static_cast<int>(imageDao_->findByPeriodId(p.id).size());
                item["url"] = "";  // PNG 未渲染, 前端可用 timeseries/comparison JSON 代替
                items.push_back(std::move(item));
            }
            else
            {
                missing.push_back(p.period_code);
            }
        }

        nlohmann::json result;
        result["task_type"] = "period_layers";
        result["layer"] = layer;
        result["project_id"] = projectId;

        // 全部期有数据即视为 "缓存完整", 直接返回成功 (不建任务, 对齐 Python 缓存命中路径)
        if (!items.empty() && missing.empty())
        {
            result["status"] = static_cast<int>(models::TaskStatus::Completed);
            result["progress"] = 100.0;
            nlohmann::json cached;
            cached["layer"] = layer;
            cached["items"] = std::move(items);
            cached["missing"] = std::move(missing);
            result["result"] = std::move(cached);
            result["params"] = {{"layer", layer}, {"cache_hit", true}};
            return result;
        }

        // 复用同项目同图层活跃任务 (pending/running), 避免重复投递 (对齐 _find_active_period_layers_task)
        for (const auto& t : taskDao_->findByProjectId(projectId))
        {
            if (t.status != models::TaskStatus::Pending && t.status != models::TaskStatus::Running)
                continue;
            if (!taskTypeMatches(t, "period_layers"))
                continue;
            try
            {
                auto tp = nlohmann::json::parse(t.params);
                if (tp.value("layer", "") == layer)
                    return taskToJson(t);
            }
            catch (const std::exception&)
            {
            }
        }

        nlohmann::json taskParams;
        taskParams["layer"] = layer;
        int taskId = createAsyncTask("period_layers", projectId, taskParams);

        std::string outputDir =
            (std::filesystem::path(dynamicRoot()) / ("p" + std::to_string(projectId)) / layer).string();
        std::thread([this, taskId, outputDir]() { runRenderPlaceholder(taskId, outputDir, "period_layers"); })
            .detach();

        models::Task task;
        task.id = taskId;
        task.project_id = projectId;
        auto created = taskDao_->findById(taskId);
        return taskToJson(created ? *created : task);
    }

    nlohmann::json DynamicService::timeSeries(const nlohmann::json& params)
    {
        int projectId = params.value("project_id", 0);
        if (projectId <= 0)
            throw common::ValidationException("缺少 project_id");

        // 期次按 end_date 升序 (对齐 Python order_by period.end_date)
        auto periods = periodDao_->findByProjectId(projectId);
        std::sort(periods.begin(), periods.end(),
                  [](const models::MonitoringPeriod& a, const models::MonitoringPeriod& b)
                  { return a.end_date < b.end_date; });
        std::map<int, const models::MonitoringPeriod*> periodById;
        for (const auto& p : periods)
            periodById[p.id] = &p;

        auto indicators = indicatorDao_->listActive("");
        std::map<int, const models::Indicator*> indById;
        for (const auto& ind : indicators)
            indById[ind.id] = &ind;

        auto values = indicatorDao_->listValues(projectId, std::nullopt, std::nullopt);

        // 按指标聚合跨期点 (对齐 assemble_multi_trend)
        std::map<std::string, nlohmann::json> grouped;  // indicator_code -> series
        for (const auto& v : values)
        {
            auto iit = indById.find(v.indicator_id);
            auto pit = periodById.find(v.period_id);
            if (iit == indById.end() || pit == periodById.end())
                continue;
            const auto& ind = *iit->second;
            const auto& period = *pit->second;

            nlohmann::json point;
            point["period_code"] = period.period_code;
            point["period_date"] = tpToDate(period.end_date);
            point["value"] = v.raw_value;
            point["label"] = v.level.empty() ? nlohmann::json(nullptr) : nlohmann::json(v.level);

            auto git = grouped.find(ind.code);
            if (git == grouped.end())
            {
                nlohmann::json s;
                s["indicator_code"] = ind.code;
                s["indicator_name"] = ind.name;
                s["dimension"] = ind.dimension;
                s["unit"] = ind.unit.has_value() ? nlohmann::json(*ind.unit) : nlohmann::json(nullptr);
                s["points"] = nlohmann::json::array();
                git = grouped.emplace(ind.code, std::move(s)).first;
            }
            git->second["points"].push_back(std::move(point));
        }

        nlohmann::json series = nlohmann::json::array();
        for (auto& [code, s] : grouped)
            series.push_back(std::move(s));

        if (series.empty())
            throw common::ValidationException("该项目暂无指标数据");

        nlohmann::json result;
        result["project_id"] = projectId;
        result["series"] = std::move(series);
        // PNG 渲染未移植: 返回结构化趋势数据, 前端自行绘图 (Python 返回 {url: /static/.../timeseries.png})
        result["url"] = nullptr;
        return result;
    }

    nlohmann::json DynamicService::comparison(const nlohmann::json& params)
    {
        int projectId = params.value("project_id", 0);
        std::string layer = toLowerStr(params.value("layer", ""));
        std::string codeA = params.value("code_a", "");
        std::string codeB = params.value("code_b", "");
        std::string mode = params.value("mode", "side");
        checkLayer(layer);
        if (projectId <= 0 || codeA.empty() || codeB.empty())
            throw common::ValidationException("缺少 project_id / code_a / code_b");
        if (mode != "side" && mode != "diff")
            throw common::ValidationException("未知 mode: " + mode + " (可选 side / diff)");

        auto periods = periodDao_->findByProjectId(projectId);
        const models::MonitoringPeriod* pa = nullptr;
        const models::MonitoringPeriod* pb = nullptr;
        for (const auto& p : periods)
        {
            if (p.period_code == codeA)
                pa = &p;
            if (p.period_code == codeB)
                pb = &p;
        }
        if (!pa || !pb)
            throw common::ValidationException("期次不存在");

        // 按图层聚合该期成果指标 (Python 渲染栅格对比 PNG, 此处返回数值对比 JSON)
        auto aggregate = [&](int periodId) -> nlohmann::json
        {
            nlohmann::json agg;
            if (layer == "fvc" || layer == "coverage")
            {
                auto vc = fvcDao_->findByPeriodId(periodId);
                agg["mean_fvc"] = vc ? nlohmann::json(vc->mean_fvc) : nlohmann::json(nullptr);
                agg["low_coverage_area"] = vc ? nlohmann::json(vc->low_coverage_area) : nlohmann::json(nullptr);
                agg["high_coverage_area"] = vc ? nlohmann::json(vc->high_coverage_area) : nlohmann::json(nullptr);
            }
            else if (layer == "erosion")
            {
                auto er = erosionDao_->findByPeriodId(periodId);
                agg["mean_modulus"] = er ? nlohmann::json(er->mean_modulus) : nlohmann::json(nullptr);
                agg["strong_area"] = er ? nlohmann::json(er->strong_area) : nlohmann::json(nullptr);
                agg["severe_area"] = er ? nlohmann::json(er->severe_area) : nlohmann::json(nullptr);
            }
            else if (layer == "disturbance")
            {
                auto ds = disturbanceDao_->findByPeriodId(periodId);
                double area = 0.0;
                for (const auto& d : ds)
                    area += d.area;
                agg["count"] = static_cast<int>(ds.size());
                agg["total_area"] = area;
            }
            else if (layer == "measure")
            {
                auto ms = measureDao_->findByPeriodId(periodId);
                agg["count"] = static_cast<int>(ms.size());
            }
            else  // landuse: 无底层数据
            {
                agg["available"] = false;
            }
            return agg;
        };

        nlohmann::json result;
        result["layer"] = layer;
        result["mode"] = mode;
        result["period_a"] = {{"period_id", pa->id}, {"period_code", pa->period_code}, {"data", aggregate(pa->id)}};
        result["period_b"] = {{"period_id", pb->id}, {"period_code", pb->period_code}, {"data", aggregate(pb->id)}};

        // diff 模式: 数值型指标差值 (b - a)
        if (mode == "diff")
        {
            nlohmann::json delta;
            const auto& a = result["period_a"]["data"];
            const auto& b = result["period_b"]["data"];
            for (auto it = b.begin(); it != b.end(); ++it)
            {
                auto ait = a.find(it.key());
                if (ait != a.end() && ait->is_number() && it->is_number())
                    delta[it.key()] = it->get<double>() - ait->get<double>();
            }
            result["delta"] = std::move(delta);
        }

        // PNG 未渲染 (Python 返回 {url, mode})
        result["url"] = nullptr;
        return result;
    }

    nlohmann::json DynamicService::animation(const nlohmann::json& params)
    {
        int projectId = params.value("project_id", 0);
        std::string layer = toLowerStr(params.value("layer", "fvc"));
        checkLayer(layer);
        if (projectId <= 0)
            throw common::ValidationException("缺少 project_id");

        double fps = params.value("fps", 2.0);

        // formats: "gif,mp4" 字符串或数组, 仅保留 gif/mp4 (对齐 Python)
        std::vector<std::string> fmts;
        if (params.contains("formats"))
        {
            if (params["formats"].is_string())
            {
                std::stringstream ss(params["formats"].get<std::string>());
                std::string item;
                while (std::getline(ss, item, ','))
                {
                    item.erase(std::remove_if(item.begin(), item.end(), [](unsigned char c) { return std::isspace(c); }),
                               item.end());
                    if (item == "gif" || item == "mp4")
                        fmts.push_back(item);
                }
            }
            else if (params["formats"].is_array())
            {
                for (const auto& f : params["formats"])
                {
                    if (f.is_string() && (f.get<std::string>() == "gif" || f.get<std::string>() == "mp4"))
                        fmts.push_back(f.get<std::string>());
                }
            }
        }
        if (fmts.empty())
            fmts = {"gif", "mp4"};

        nlohmann::json taskParams;
        taskParams["layer"] = layer;
        taskParams["fps"] = fps;
        taskParams["formats"] = fmts;
        int taskId = createAsyncTask("change_animation", projectId, taskParams);

        std::string outputDir =
            (std::filesystem::path(dynamicRoot()) / ("p" + std::to_string(projectId)) / "animation").string();
        std::thread([this, taskId, outputDir]() { runRenderPlaceholder(taskId, outputDir, "change_animation"); })
            .detach();

        auto created = taskDao_->findById(taskId);
        return taskToJson(created ? *created : models::Task{});
    }

    void DynamicService::runRenderPlaceholder(int taskId, std::string outputDir, std::string taskType)
    {
        try
        {
            taskDao_->updateStatus(taskId, models::TaskStatus::Running, 10.0);
            std::filesystem::create_directories(outputDir);

            // PNG/GIF/MP4 渲染算法 (matplotlib/rasterio/imageio) 未移植: 写占位说明, 不产出媒体文件。
            // 不再"假成功"置 Completed (无媒体成果会误导前端), 改显式 Failed。
            nlohmann::json placeholder;
            placeholder["task_type"] = taskType;
            placeholder["status"] = "not_implemented";
            placeholder["note"] = "动态图渲染待移植, 未生成媒体文件";
            std::ofstream out((std::filesystem::path(outputDir) / (taskType + ".json")).string());
            out << placeholder.dump(2);

            taskDao_->updateResult(taskId, outputDir);
            taskDao_->updateStatusAndMessage(taskId, models::TaskStatus::Failed, 10.0,
                                             taskType + ": 动态图渲染未移植 (C++ 后端未实现), 无媒体成果");
            utils::Logger::warn("Dynamic: {} task {} marked failed (not implemented)", taskType, taskId);
        }
        catch (const std::exception& e)
        {
            utils::Logger::error("Dynamic: {} task {} failed: {}", taskType, taskId, e.what());
            try
            {
                taskDao_->updateStatus(taskId, models::TaskStatus::Failed, 0.0);
            }
            catch (...)
            {
            }
        }
    }

    std::string DynamicService::getFile(const nlohmann::json& params)
    {
        std::string path = params.value("path", "");
        if (path.empty())
            throw common::ValidationException("缺少 path");

        std::error_code ec;
        auto base = std::filesystem::weakly_canonical(std::filesystem::path(dynamicRoot()), ec);
        auto target = std::filesystem::weakly_canonical(std::filesystem::absolute(path), ec);

        // 路径越界防护: target 必须在 results/dynamic 之下 (对齐 Python is_relative_to)
        auto baseStr = base.string();
        auto targetStr = target.string();
        bool inside = targetStr.size() >= baseStr.size() && targetStr.compare(0, baseStr.size(), baseStr) == 0 &&
                      (targetStr.size() == baseStr.size() || targetStr[baseStr.size()] == '\\' ||
                       targetStr[baseStr.size()] == '/');
        if (!inside)
            throw common::ValidationException("路径越界");

        if (!std::filesystem::exists(target, ec))
            throw common::FileNotFoundException("文件不存在: " + path);

        return targetStr;
    }

    nlohmann::json DynamicService::taskToJson(const models::Task& task)
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
