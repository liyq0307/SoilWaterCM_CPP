#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../models/zone.h"
#include "dao_base.h"

namespace soilwatercm::dao
{

    /**
     * @brief 防治分区数据访问对象
     *
     * 管理两张表: zone_stats (分区治理统计: 设计面积/措施量/进度) 和
     * zone_boundaries (分区边界几何)。另提供扰动图斑、水保措施与分区边界
     * 的空间套合辅助查询。
     */
    class ZoneDao : public DaoBase<models::ZoneStats>
    {
    public:
        /**
         * @brief 构造, 注入数据库仓库
         * @param repo PostgreSQL 访问仓库
         */
        explicit ZoneDao(std::shared_ptr<repository::PostgresRepository> repo);

        // ---- ZoneStats ----

        /**
         * @brief 按 id 查分区统计
         * @param id 分区统计主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::ZoneStats> findById(int id) override;

        /**
         * @brief 分页列出所有分区统计
         * @param page 页码, 从 1 开始
         * @param pageSize 每页条数
         * @param orderBy 排序列名
         * @param ascending 是否升序
         * @return 当前页记录列表
         */
        std::vector<models::ZoneStats> list(int page = 1, int pageSize = 20, const std::string& orderBy = "id",
                                            bool ascending = true) override;

        /**
         * @brief 新增分区统计
         * @param stats 待插入记录 (created_at/updated_at 由调用方填好)
         * @return 新记录 id; 失败返回 -1
         */
        int insert(const models::ZoneStats& stats) override;

        /**
         * @brief 按 id 更新分区统计全字段
         * @param stats 记录 (须含 id)
         * @return 更新成功返回 true
         */
        bool update(const models::ZoneStats& stats) override;

        /**
         * @brief 按 id 删除分区统计
         * @param id 分区统计主键
         * @return 删除成功返回 true
         */
        bool remove(int id) override;

        /**
         * @brief 分区统计总条数
         * @return 表内记录数
         */
        int count() override;

        /**
         * @brief 按项目查分区统计
         * @param projectId 项目 id
         * @param periodId 期次 id; 为空则不过滤
         * @param zone 分区代码; 为空则不过滤
         * @return 匹配记录, 按 zone 排序, 最多 50 条
         */
        std::vector<models::ZoneStats> findByProject(int projectId, std::optional<int> periodId,
                                                     const std::string& zone);

        /**
         * @brief 查项目下指定分区的统计 (取一条)
         * @param projectId 项目 id
         * @param zone 分区代码
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::ZoneStats> findByProjectAndZone(int projectId, const std::string& zone);

        /**
         * @brief 只更新设计面积一列
         * @param id 分区统计主键
         * @param areaHa 防治责任范围面积 (公顷)
         * @return 更新成功返回 true
         */
        bool updateDesignedArea(int id, double areaHa);

        // ---- ZoneBoundary ----

        /**
         * @brief 列出项目的全部分区边界
         * @param projectId 项目 id
         * @return 边界记录 (几何以 WKT 读出), 按 zone 排序
         */
        std::vector<models::ZoneBoundary> listBoundaries(int projectId);

        /**
         * @brief 新增分区边界
         * @param boundary 边界记录; 几何以 ST_GeomFromText 写入 (SRID 4326)
         * @return 新记录 id; 失败返回 -1
         */
        int insertBoundary(const models::ZoneBoundary& boundary);

        /**
         * @brief 删除项目的全部分区边界 (重传前清理)
         * @param projectId 项目 id
         * @return 删除成功返回 true
         */
        bool deleteBoundariesByProject(int projectId);

        /**
         * @brief 判定几何所属分区 (空间套合)
         *
         * 质心落入的分区优先; 否则取相交面积最大的分区。
         * @param projectId 项目 id
         * @param geomWkt 待判定几何的 WKT
         * @return 分区代码; 无匹配返回空串
         */
        std::string pickZoneForGeometry(int projectId, const std::string& geomWkt);

        /**
         * @brief 列出待套合的扰动图斑 (id, 几何 WKT)
         * @param projectId 项目 id (经 monitoring_periods 关联)
         * @param periodId 期次 id; 为空则项目全期次
         * @return (id, WKT) 对列表; 几何为空的记录跳过
         */
        std::vector<std::pair<int, std::string>> listDisturbanceGeoms(int projectId, std::optional<int> periodId);

        /**
         * @brief 列出待套合的水保措施 (id, 几何 WKT)
         * @param projectId 项目 id (经 monitoring_periods 关联)
         * @param periodId 期次 id; 为空则项目全期次
         * @return (id, WKT) 对列表; 几何为空的记录跳过
         */
        std::vector<std::pair<int, std::string>> listMeasureGeoms(int projectId, std::optional<int> periodId);

        /**
         * @brief 回填扰动图斑的 zone 字段
         * @param id 扰动图斑主键
         * @param zone 分区代码
         * @return 更新成功返回 true
         */
        bool setDisturbanceZone(int id, const std::string& zone);

        /**
         * @brief 回填水保措施的 zone 字段
         * @param id 措施主键
         * @param zone 分区代码
         * @return 更新成功返回 true
         */
        bool setMeasureZone(int id, const std::string& zone);

    private:
        /**
         * @brief 行数据 -> 分区统计实体
         * @param row 查询结果单行 (列名 -> 文本值)
         * @return 实体
         */
        models::ZoneStats mapRowToStats(const std::map<std::string, std::string>& row);

        /**
         * @brief 行数据 -> 分区边界实体
         * @param row 查询结果单行 (几何列为 WKT 文本)
         * @return 实体
         */
        models::ZoneBoundary mapRowToBoundary(const std::map<std::string, std::string>& row);
    };

}  // namespace soilwatercm::dao
