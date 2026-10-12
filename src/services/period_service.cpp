#include "period_service.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "../common/exceptions.h"
#include "../infrastructure/config_manager.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{

    namespace
    {
        std::string toLowerStr(const std::string& s)
        {
            std::string out = s;
            std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
            return out;
        }

        std::string trimStr(const std::string& s)
        {
            size_t b = s.find_first_not_of(" \t\r\n");
            if (b == std::string::npos)
                return "";
            size_t e = s.find_last_not_of(" \t\r\n");
            return s.substr(b, e - b + 1);
        }

        // UTC ISO8601 (Python datetime.now(timezone.utc).isoformat() 语义)
        std::string utcNowIso()
        {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::tm tm = {};
#ifdef _WIN32
            gmtime_s(&tm, &time);
#else
            gmtime_r(&time, &tm);
#endif
            char buf[64];
            std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
            return std::string(buf);
        }

        // 归档清单结构版本 (对应 Python settings.VERSION)
        constexpr const char* kManifestVersion = "1.0.0";
    }  // namespace

    std::string periodTypeToString(models::PeriodType type)
    {
        switch (type)
        {
            case models::PeriodType::Monthly:
                return "monthly";
            case models::PeriodType::Quarterly:
                return "quarterly";
            case models::PeriodType::Annual:
                return "yearly";
            case models::PeriodType::Emergency:
                return "emergency";
            default:
                return "monthly";
        }
    }

    std::optional<models::PeriodType> parsePeriodType(const std::string& raw)
    {
        std::string s = toLowerStr(trimStr(raw));
        if (s == "monthly")
            return models::PeriodType::Monthly;
        if (s == "quarterly")
            return models::PeriodType::Quarterly;
        if (s == "yearly" || s == "annual")
            return models::PeriodType::Annual;
        if (s == "emergency")
            return models::PeriodType::Emergency;
        return std::nullopt;
    }

    std::string projectStatusToString(models::ProjectStatus status)
    {
        switch (status)
        {
            case models::ProjectStatus::Active:
                return "active";
            case models::ProjectStatus::Completed:
                return "completed";
            case models::ProjectStatus::Archived:
                return "archived";
            default:
                return "active";
        }
    }

    std::optional<models::ProjectStatus> parseProjectStatus(const std::string& raw)
    {
        std::string s = toLowerStr(trimStr(raw));
        if (s == "archived")
            return models::ProjectStatus::Archived;
        if (s == "completed")
            return models::ProjectStatus::Completed;
        if (s == "active")
            return models::ProjectStatus::Active;
        // Python 工作流状态 (planning/acquiring/processing/extracted/evaluated/reported)
        // 在 C++ 三值枚举下统一归入 active (非归档的进行中状态)
        if (s == "planning" || s == "acquiring" || s == "processing" || s == "extracted" || s == "evaluated" ||
            s == "reported")
            return models::ProjectStatus::Active;
        return std::nullopt;
    }

    std::chrono::system_clock::time_point PeriodService::parseDateTime(const std::string& raw)
    {
        std::string s = trimStr(raw);
        if (s.empty())
            throw common::ValidationException("日期不能为空: " + raw);

        for (const char* fmt : {"%Y-%m-%d %H:%M:%S", "%Y-%m-%d"})
        {
            std::tm tm = {};
            std::istringstream ss(s);
            if (ss >> std::get_time(&tm, fmt))
                return std::chrono::system_clock::from_time_t(std::mktime(&tm));
        }
        throw common::ValidationException("无效日期格式: " + raw);
    }

    std::string PeriodService::formatDateTime(const std::chrono::system_clock::time_point& tp)
    {
        auto time = std::chrono::system_clock::to_time_t(tp);
        std::tm tm = {};
#ifdef _WIN32
        localtime_s(&tm, &time);
#else
        localtime_r(&time, &tm);
#endif
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        return std::string(buf);
    }

    PeriodService::PeriodService(std::shared_ptr<dao::MonitoringPeriodDao> periodDao,
                                 std::shared_ptr<dao::DisturbanceDao> disturbanceDao,
                                 std::shared_ptr<dao::VegetationCoverageDao> vegetationDao,
                                 std::shared_ptr<dao::ErosionResultDao> erosionDao,
                                 std::shared_ptr<dao::MeasureDao> measureDao,
                                 std::shared_ptr<dao::AnomalyDao> anomalyDao, std::shared_ptr<dao::ReportDao> reportDao)
        : periodDao_(std::move(periodDao)), disturbanceDao_(std::move(disturbanceDao)),
          vegetationDao_(std::move(vegetationDao)), erosionDao_(std::move(erosionDao)), measureDao_(std::move(measureDao)),
          anomalyDao_(std::move(anomalyDao)), reportDao_(std::move(reportDao))
    {
    }

    std::vector<models::MonitoringPeriod> PeriodService::list(std::optional<int> projectId)
    {
        if (projectId.has_value())
            return periodDao_->findByProjectId(*projectId);
        // 全量: 按 start_date 倒序 (Python order_by(start_date.desc()) 语义)
        return periodDao_->list(1, 1000, "start_date", false);
    }

    models::MonitoringPeriod PeriodService::create(const models::MonitoringPeriod& period)
    {
        if (trimStr(period.period_code).empty())
            throw common::ValidationException("period_code 不能为空");
        if (period.project_id <= 0)
            throw common::ValidationException("project_id 无效");
        if (period.end_date < period.start_date)
            throw common::ValidationException("end_date 不能早于 start_date");

        models::MonitoringPeriod rec = period;
        int id = periodDao_->insert(rec);
        if (id < 0)
            throw common::DatabaseException("创建监测期失败");
        rec.id = id;
        utils::Logger::info("Monitoring period created: id={}, code={}", rec.id, rec.period_code);
        return rec;
    }

    std::optional<models::MonitoringPeriod> PeriodService::getById(int id)
    {
        return periodDao_->findById(id);
    }

    std::optional<models::MonitoringPeriod> PeriodService::updateStatus(int id, const std::string& statusStr)
    {
        auto existing = periodDao_->findById(id);
        if (!existing)
            return std::nullopt;

        auto status = parseProjectStatus(statusStr);
        if (!status)
            throw common::ValidationException("无效监测期状态: " + statusStr);

        models::MonitoringPeriod rec = *existing;
        rec.status = *status;
        periodDao_->update(rec);
        return periodDao_->findById(id);
    }

    std::filesystem::path PeriodService::archiveRoot() const
    {
        return std::filesystem::path(infrastructure::ConfigManager::instance().getStorageBaseDir()) / "archive";
    }

    nlohmann::json PeriodService::archivePeriod(int id)
    {
        auto periodOpt = periodDao_->findById(id);
        if (!periodOpt)
            throw common::NotFoundException("监测期不存在 id=" + std::to_string(id));

        const auto& period = *periodOpt;
        int pid = period.project_id;

        // ---- 统计各成果数量 (Python archive_service 语义) ----
        nlohmann::json stats;
        stats["disturbances"] = static_cast<int>(disturbanceDao_->findByPeriodId(id).size());
        stats["vegetation"] = vegetationDao_->findByPeriodId(id).has_value() ? 1 : 0;
        stats["erosion"] = erosionDao_->findByPeriodId(id).has_value() ? 1 : 0;
        stats["measures"] = static_cast<int>(measureDao_->findByPeriodId(id).size());

        int anomalyCount = 0;
        for (const auto& a : anomalyDao_->findByProjectId(pid))
        {
            if (a.period_id == id)
                ++anomalyCount;
        }
        stats["anomalies"] = anomalyCount;

        // reports 模型仅有 period_ids 数组且 DAO 未解析, 无法按期过滤 -> 按项目统计 (与 Python 有偏差)
        auto reports = reportDao_->findByProjectId(pid);
        stats["reports"] = static_cast<int>(reports.size());

        // ---- 归档目录 ----
        std::filesystem::path outDir = archiveRoot() / ("period_" + std::to_string(id));
        std::error_code ec;
        std::filesystem::create_directories(outDir, ec);
        if (ec)
            throw common::FileException("创建归档目录失败: " + outDir.string() + " (" + ec.message() + ")");

        // ---- 拷贝报告文件 (缺失/失败登记 archived=false, 不报错) ----
        nlohmann::json reportFiles = nlohmann::json::array();
        for (const auto& r : reports)
        {
            for (const auto& [pathStr, fmt] : std::vector<std::pair<std::string, std::string>>{
                     {r.docx_path, "docx"}, {r.pdf_path, "pdf"}})
            {
                if (trimStr(pathStr).empty())
                    continue;

                nlohmann::json entry;
                entry["report_id"] = r.id;
                entry["title"] = r.title;
                entry["format"] = fmt;
                entry["source"] = pathStr;
                entry["archived"] = false;

                std::filesystem::path src(pathStr);
                if (std::filesystem::exists(src))
                {
                    std::error_code szEc;
                    auto bytes = std::filesystem::file_size(src, szEc);
                    entry["size_kb"] = szEc ? 0.0 : static_cast<double>(bytes) / 1024.0;

                    std::filesystem::path dst = outDir / src.filename();
                    std::error_code cpEc;
                    std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, cpEc);
                    if (!cpEc)
                    {
                        entry["archived"] = true;
                        entry["archived_path"] = dst.string();
                    }
                    else
                    {
                        utils::Logger::warn("报告归档拷贝失败 {}: {}", src.string(), cpEc.message());
                    }
                }
                else
                {
                    entry["size_kb"] = nullptr;
                }
                reportFiles.push_back(entry);
            }
        }

        // ---- 组装并写 manifest.json ----
        nlohmann::json manifest;
        manifest["period_id"] = period.id;
        manifest["period_code"] = period.period_code;
        manifest["project_id"] = period.project_id;
        manifest["period_type"] = periodTypeToString(period.type);
        manifest["archived_at"] = utcNowIso();
        manifest["stats"] = stats;
        manifest["report_files"] = reportFiles;
        manifest["version"] = kManifestVersion;

        std::ofstream ofs(outDir / "manifest.json", std::ios::binary | std::ios::trunc);
        if (!ofs.is_open())
            throw common::FileException("写入归档清单失败: " + (outDir / "manifest.json").string());
        ofs << manifest.dump(2);
        ofs.close();

        // ---- 状态置 archived ----
        models::MonitoringPeriod updated = period;
        updated.status = models::ProjectStatus::Archived;
        periodDao_->update(updated);

        utils::Logger::info("监测期归档完成 period={} stats={}", id, stats.dump());
        return manifest;
    }

    nlohmann::json PeriodService::listArchives(std::optional<int> projectId)
    {
        nlohmann::json out = nlohmann::json::array();
        std::filesystem::path root = archiveRoot();
        if (!std::filesystem::exists(root))
            return out;

        std::vector<std::filesystem::path> dirs;
        std::error_code ec;
        for (const auto& e : std::filesystem::directory_iterator(root, ec))
        {
            if (!e.is_directory())
                continue;
            if (e.path().filename().string().rfind("period_", 0) == 0)
                dirs.push_back(e.path());
        }
        std::sort(dirs.begin(), dirs.end(),
                  [](const std::filesystem::path& a, const std::filesystem::path& b) { return a < b; });

        for (const auto& d : dirs)
        {
            std::filesystem::path mpath = d / "manifest.json";
            if (!std::filesystem::exists(mpath))
                continue;
            std::ifstream ifs(mpath, std::ios::binary);
            if (!ifs.is_open())
                continue;
            try
            {
                nlohmann::json data = nlohmann::json::parse(ifs);
                if (!projectId.has_value() || data.value("project_id", -1) == *projectId)
                    out.push_back(data);
            }
            catch (const std::exception&)
            {
                continue;
            }
        }
        return out;
    }

    std::optional<nlohmann::json> PeriodService::readManifest(int id)
    {
        std::filesystem::path p = archiveRoot() / ("period_" + std::to_string(id)) / "manifest.json";
        if (!std::filesystem::exists(p))
            return std::nullopt;
        std::ifstream ifs(p, std::ios::binary);
        if (!ifs.is_open())
            return std::nullopt;
        try
        {
            return nlohmann::json::parse(ifs);
        }
        catch (const std::exception& e)
        {
            utils::Logger::warn("读取归档清单失败 {}: {}", p.string(), e.what());
            return std::nullopt;
        }
    }

}  // namespace soilwatercm::services
