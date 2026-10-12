#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../models/ground_truth.h"
#include "dao_base.h"

namespace soilwatercm::dao
{

    /**
     * @brief 地面实测样点数据访问对象
     *
     * 对应 ground_truths 表: 地面实测样点 (类型、位置、实测值、对比的遥感值、
     * 精度是否达标)。
     */
    class GroundTruthDao : public DaoBase<models::GroundTruth>
    {
    public:
        /**
         * @brief 构造, 注入数据库仓库
         * @param repo PostgreSQL 访问仓库
         */
        explicit GroundTruthDao(std::shared_ptr<repository::PostgresRepository> repo);

        /**
         * @brief 按 id 查样点
         * @param id 样点主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::GroundTruth> findById(int id) override;

        /**
         * @brief 分页列出所有样点
         * @param page 页码, 从 1 开始
         * @param pageSize 每页条数
         * @param orderBy 排序列名
         * @param ascending 是否升序
         * @return 当前页记录列表
         */
        std::vector<models::GroundTruth> list(int page = 1, int pageSize = 20, const std::string& orderBy = "id",
                                              bool ascending = true) override;

        /**
         * @brief 新增样点
         * @param gt 待插入记录; 位置以 ST_GeomFromText 写入 (SRID 4326)
         * @return 新记录 id; 失败返回 -1
         */
        int insert(const models::GroundTruth& gt) override;

        /**
         * @brief 按 id 更新样点全部字段
         * @param gt 记录 (须含 id); 位置以 ST_GeomFromText 写入 (SRID 4326)
         * @return 更新成功返回 true
         */
        bool update(const models::GroundTruth& gt) override;

        /**
         * @brief 按 id 删除样点
         * @param id 样点主键
         * @return 删除成功返回 true
         */
        bool remove(int id) override;

        /**
         * @brief 样点总数
         * @return 表内记录数
         */
        int count() override;

        /**
         * @brief 可选过滤 + 分页列出样点
         * @param projectId 项目 id; 为空则不过滤
         * @param periodId 期次 id; 为空则不过滤
         * @param sampleType 样点类型; 为空则不过滤
         * @param page 页码, 从 1 开始
         * @param pageSize 每页条数
         * @return 当前页记录, 按调查日期倒序
         */
        std::vector<models::GroundTruth> list(std::optional<int> projectId, std::optional<int> periodId,
                                              const std::string& sampleType, int page = 1, int pageSize = 50);

        /**
         * @brief 按项目查样点
         * @param projectId 项目 id
         * @return 匹配记录, 按调查日期倒序
         */
        std::vector<models::GroundTruth> findByProject(int projectId);

        /**
         * @brief 从遥感目标表读取提取值
         * @param targetTable 目标表名; 仅支持 disturbances (取 area) 和
         *        vegetation_coverages (取 mean_fvc), 其余返回 nullopt
         * @param targetId 目标记录 id
         * @return 提取值; 记录不存在或表名不支持返回 nullopt
         */
        std::optional<double> fetchRsValue(const std::string& targetTable, int targetId);

    private:
        /// @brief 行数据 -> 实体 (位置以 WKT 读出)
        models::GroundTruth mapRow(const std::map<std::string, std::string>& row);
    };

}  // namespace soilwatercm::dao
