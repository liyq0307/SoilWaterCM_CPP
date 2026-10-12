#pragma once
#include "../models/measure.h"
#include "dao_base.h"
namespace soilwatercm::dao
{
    /**
     * @brief 水保措施数据访问对象
     *
     * 对应 measures 表: 水保措施 (类型、几何、长度/面积、状态、完好程度),
     * 按期次归属项目。
     */
    class MeasureDao : public DaoBase<models::Measure>
    {
    public:
        /**
         * @brief 构造, 注入数据库仓库
         * @param repo PostgreSQL 访问仓库
         */
        explicit MeasureDao(std::shared_ptr<repository::PostgresRepository> repo);

        /**
         * @brief 按 id 查措施
         * @param id 措施主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::Measure> findById(int id) override;

        /**
         * @brief 按期次查措施
         * @param periodId 期次 id
         * @return 匹配记录列表
         */
        std::vector<models::Measure> findByPeriodId(int periodId);

        /**
         * @brief 按项目查措施 (对齐 Python GET /extraction/measures)
         *
         * 通过 period_id join monitoring_periods 归属项目。
         * @param projectId 项目 id
         * @param periodId 期次 id; 为空则项目全期次
         * @return 匹配记录, 按 id 倒序, 最多 2000 条
         */
        virtual std::vector<models::Measure> findByProject(int projectId, std::optional<int> periodId = std::nullopt);

        /**
         * @brief 分页列出所有措施
         * @param page 页码, 从 1 开始
         * @param pageSize 每页条数
         * @param orderBy 排序列名
         * @param ascending 是否升序
         * @return 当前页记录列表
         */
        std::vector<models::Measure> list(int page = 1, int pageSize = 20, const std::string& orderBy = "id",
                                          bool ascending = true) override;

        /**
         * @brief 新增措施
         * @param m 待插入记录; 几何以 ST_GeomFromText 写入 (SRID 4326)
         * @return 新记录 id; 失败返回 -1
         */
        int insert(const models::Measure& m) override;

        /**
         * @brief 按 id 更新措施状态和完好程度
         * @param m 记录 (须含 id)
         * @return 更新成功返回 true
         */
        bool update(const models::Measure& m) override;

        /**
         * @brief 按 id 删除措施
         * @param id 措施主键
         * @return 删除成功返回 true
         */
        bool remove(int id) override;

        /**
         * @brief 措施总数
         * @return 表内记录数
         */
        int count() override;

    private:
        /// @brief 行数据 -> 实体 (几何以 WKT 读出)
        models::Measure mapRowToMeasure(const std::map<std::string, std::string>& row);
    };
}  // namespace soilwatercm::dao
