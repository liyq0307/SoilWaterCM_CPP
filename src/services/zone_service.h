#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../dao/zone_dao.h"
#include "../models/zone.h"
#include <nlohmann/json.hpp>

namespace soilwatercm::services
{

    /**
     * @brief 分区代码解析 (标准 code 或中文名 -> code)
     * @param raw 原始字符串, 如 "waste_dump" 或 "排土场区" (首尾空白忽略)
     * @return 标准分区代码; 无法识别返回 nullopt
     */
    std::optional<std::string> resolveZoneCode(const std::string& raw);

    /**
     * @brief 分区管理服务
     *
     * 维护分区统计 (设计指标/进度) 与分区边界, 支持从矢量文件套合空间关系,
     * 回填扰动/措施记录的所属分区, 并按分区代码初始化设计指标。
     */
    class ZoneService
    {
    public:
        /**
         * @brief 构造, 注入分区数据访问对象
         * @param zoneDao 分区 DAO
         */
        explicit ZoneService(std::shared_ptr<dao::ZoneDao> zoneDao);

        // ---- ZoneStats CRUD ----

        /**
         * @brief 列分区统计
         * @param projectId 项目 id
         * @param periodId 期次 id; 为空则不过滤
         * @param zone 分区代码; 为空则不过滤
         * @return 匹配的分区统计列表
         */
        std::vector<models::ZoneStats> listStats(int projectId, std::optional<int> periodId, const std::string& zone);

        /**
         * @brief 新建分区统计
         * @param stats 待插入记录 (created_at/updated_at 由服务自动填)
         * @return 含新 id 的记录
         * @throw ValidationException 分区代码无效时
         * @throw DatabaseException 入库失败时
         */
        models::ZoneStats createStats(const models::ZoneStats& stats);

        /**
         * @brief 按 patch 字段更新分区统计
         * @param id 分区统计主键
         * @param patch 仅更新其中出现的字段 (period_id/zone/designed_area_ha/
         *              designed_measures/progress_status/note)
         * @return 更新后记录; id 不存在返回 nullopt
         * @throw ValidationException patch 中 zone 代码无效时
         */
        std::optional<models::ZoneStats> updateStats(int id, const nlohmann::json& patch);

        /**
         * @brief 删除分区统计
         * @param id 分区统计主键
         * @return 删除成功返回 true
         */
        bool deleteStats(int id);

        // ---- ZoneBoundary ----

        /**
         * @brief 列项目分区边界
         * @param projectId 项目 id
         * @return 边界记录列表
         */
        std::vector<models::ZoneBoundary> listBoundaries(int projectId);

        /**
         * @brief 覆盖式写入分区边界 (GeoJSON FeatureCollection)
         * @param projectId 项目 id
         * @param featureCollection GeoJSON, 每个 feature 的 properties 须含分区标识
         *        (zone/Zone/分区/name/Name 任一字段)
         * @return {saved: [已存分区代码], skipped: [未识别的标识]}
         * @throw ValidationException GeoJSON 无 features 时
         */
        nlohmann::json saveBoundaries(int projectId, const nlohmann::json& featureCollection);

        /**
         * @brief 覆盖式写入分区边界 (来自矢量文件解析结果)
         *
         * 按属性中的分区标识分组, 同组多边形合并为 MultiPolygon 后逐分区入库。
         * @param projectId 项目 id
         * @param features (几何 WKT, 属性表) 列表
         * @return {saved: [已存分区代码]}
         * @throw ValidationException 无任何有效分区几何时
         */
        nlohmann::json saveBoundariesFromFeatures(
            int projectId, const std::vector<std::pair<std::string, std::map<std::string, std::string>>>& features);

        /**
         * @brief 空间套合, 回填扰动/措施的 zone 字段
         * @param projectId 项目 id
         * @param periodId 期次 id; 为空则项目全期次
         * @return {zones, disturbances_assigned, measures_assigned, unmatched};
         *         无边界时 zones 为 0 且 detail 提示先上传
         */
        nlohmann::json assignZones(int projectId, std::optional<int> periodId);

        /**
         * @brief 初始化/补全分区设计指标
         *
         * 逐分区代码建档: 已有记录只补缺失的设计面积, 没有则新建。
         * 面积优先取 zoneArea 入参, 其次取边界面积。
         * @param projectId 项目 id
         * @param periodId 期次 id; 为空则不绑期次
         * @param zoneArea 分区代码 -> 设计面积 (公顷), 可缺省
         * @return {created, skipped, with_boundary}
         */
        nlohmann::json initZoneStats(int projectId, std::optional<int> periodId,
                                     const std::map<std::string, std::optional<double>>& zoneArea = {});

        /**
         * @brief 分区代码 -> 中文名映射
         * @return JSON 对象, 如 {"1": "Ⅰ区", ...}
         */
        static nlohmann::json zoneCodesJson();

    private:
        std::shared_ptr<dao::ZoneDao> zoneDao_;
    };

}  // namespace soilwatercm::services
