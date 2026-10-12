#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "../dao/anomaly_dao.h"
#include "../dao/disturbance_dao.h"
#include "../dao/erosion_result_dao.h"
#include "../dao/measure_dao.h"
#include "../dao/monitoring_period_dao.h"
#include "../dao/report_dao.h"
#include "../dao/vegetation_coverage_dao.h"
#include "../models/monitoring_period.h"

namespace soilwatercm::services
{

    /**
     * @brief 监测期类型 -> 字符串 (monthly/quarterly/yearly/emergency)
     * @param type 期次类型枚举
     * @return 小写字符串
     */
    std::string periodTypeToString(models::PeriodType type);

    /**
     * @brief 字符串 -> 监测期类型
     * @param raw 原始字符串 (大小写不敏感)
     * @return 类型枚举; 无法识别返回 nullopt
     */
    std::optional<models::PeriodType> parsePeriodType(const std::string& raw);

    /**
     * @brief 项目/监测期状态 -> 字符串 (active/completed/archived)
     * @param status 状态枚举
     * @return 字符串
     */
    std::string projectStatusToString(models::ProjectStatus status);

    /**
     * @brief 字符串 -> 项目/监测期状态
     * @param raw 原始字符串, 兼容 planning/acquiring 等工作流字符串
     * @return 状态枚举; 无法识别返回 nullopt
     */
    std::optional<models::ProjectStatus> parseProjectStatus(const std::string& raw);

    /**
     * @brief 监测期管理与历史归档服务
     *
     * 负责监测期 CRUD 与状态流转。归档时统计扰动/FVC/侵蚀/措施/异常各成果数量,
     * 拷贝报告文件到 archive/period_{id}/ 目录并写 manifest.json 清单。
     */
    class PeriodService
    {
    public:
        /**
         * @brief 构造, 注入监测期与各成果 DAO
         * @param periodDao 监测期 DAO
         * @param disturbanceDao 扰动图斑 DAO (归档计数)
         * @param vegetationDao 植被覆盖度 DAO (归档计数)
         * @param erosionDao 侵蚀结果 DAO (归档计数)
         * @param measureDao 水保措施 DAO (归档计数)
         * @param anomalyDao 异常记录 DAO (归档计数)
         * @param reportDao 报告 DAO (归档拷贝报告文件)
         */
        PeriodService(std::shared_ptr<dao::MonitoringPeriodDao> periodDao,
                      std::shared_ptr<dao::DisturbanceDao> disturbanceDao,
                      std::shared_ptr<dao::VegetationCoverageDao> vegetationDao,
                      std::shared_ptr<dao::ErosionResultDao> erosionDao, std::shared_ptr<dao::MeasureDao> measureDao,
                      std::shared_ptr<dao::AnomalyDao> anomalyDao, std::shared_ptr<dao::ReportDao> reportDao);

        // ---- 监测期 CRUD ----

        /**
         * @brief 列监测期
         * @param projectId 项目 id; 指定则按项目查, 否则全量按期次开始时间倒序
         * @return 监测期列表
         */
        std::vector<models::MonitoringPeriod> list(std::optional<int> projectId);

        /**
         * @brief 创建监测期
         * @param period 待插入记录
         * @return 含新 id 的记录
         * @throw ValidationException period_code 为空 / project_id 无效 /
         *        end_date 早于 start_date 时
         * @throw DatabaseException 入库失败时
         */
        models::MonitoringPeriod create(const models::MonitoringPeriod& period);

        /**
         * @brief 按 id 查监测期
         * @param id 监测期主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::MonitoringPeriod> getById(int id);

        /**
         * @brief 更新监测期状态
         * @param id 监测期主键
         * @param statusStr 状态字符串
         * @return 更新后记录; 监测期不存在返回 nullopt
         * @throw ValidationException 状态字符串无法解析时
         */
        std::optional<models::MonitoringPeriod> updateStatus(int id, const std::string& statusStr);

        // ---- 历史归档 ----

        /**
         * @brief 归档监测期
         *
         * 统计各成果数量 -> 拷贝报告文件到 archive/period_{id}/ ->
         * 写 manifest.json -> 状态置 archived。
         * @param id 监测期主键
         * @return 归档清单 JSON (manifest 内容)
         * @throw NotFoundException 监测期不存在时
         * @throw FileException 创建归档目录或写清单失败时
         */
        nlohmann::json archivePeriod(int id);

        /**
         * @brief 已归档监测期清单
         * @param projectId 项目 id; 为空则扫描全部归档
         * @return 各期 manifest 列表 (读归档目录下各 manifest.json)
         */
        nlohmann::json listArchives(std::optional<int> projectId);

        /**
         * @brief 读取某期归档清单
         * @param id 监测期主键
         * @return manifest JSON; 未归档返回 nullopt
         */
        std::optional<nlohmann::json> readManifest(int id);

        /**
         * @brief 解析日期字符串 ("YYYY-MM-DD" 或 "YYYY-MM-DD HH:MM:SS")
         * @param raw 原始字符串
         * @return 时间点 (按本地时区解释)
         * @throw ValidationException 字符串为空或格式无效时
         */
        static std::chrono::system_clock::time_point parseDateTime(const std::string& raw);

        /**
         * @brief 格式化时间点为 "YYYY-MM-DD HH:MM:SS"
         * @param tp 时间点
         * @return 字符串
         */
        static std::string formatDateTime(const std::chrono::system_clock::time_point& tp);

    private:
        /** @brief 归档根目录 (存储 results 目录下的 archive/) */
        std::filesystem::path archiveRoot() const;

        std::shared_ptr<dao::MonitoringPeriodDao> periodDao_;
        std::shared_ptr<dao::DisturbanceDao> disturbanceDao_;
        std::shared_ptr<dao::VegetationCoverageDao> vegetationDao_;
        std::shared_ptr<dao::ErosionResultDao> erosionDao_;
        std::shared_ptr<dao::MeasureDao> measureDao_;
        std::shared_ptr<dao::AnomalyDao> anomalyDao_;
        std::shared_ptr<dao::ReportDao> reportDao_;
    };

}  // namespace soilwatercm::services
