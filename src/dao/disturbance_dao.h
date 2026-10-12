#pragma once
#include "../models/disturbance.h"
#include "dao_base.h"

namespace soilwatercm::dao
{

    /**
     * @brief 扰动图斑数据访问对象
     *
     * 对应 disturbances 表: 遥感提取的扰动图斑 (编码、类型、几何、面积/周长、
     * 置信度), 按期次归属项目。
     */
    class DisturbanceDao : public DaoBase<models::Disturbance>
    {
    public:
        /**
         * @brief 构造, 注入数据库仓库
         * @param repo PostgreSQL 访问仓库
         */
        explicit DisturbanceDao(std::shared_ptr<repository::PostgresRepository> repo);

        /**
         * @brief 按 id 查扰动
         * @param id 扰动主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::Disturbance> findById(int id) override;

        /**
         * @brief 按期次查扰动
         * @param periodId 期次 id
         * @return 匹配记录列表
         */
        virtual std::vector<models::Disturbance> findByPeriodId(int periodId);

        /**
         * @brief 按项目查扰动 (对齐 Python GET /extraction/disturbances)
         *
         * 通过 period_id join monitoring_periods 归属项目。
         * @param projectId 项目 id
         * @param periodId 期次 id; 为空则项目全期次
         * @return 匹配记录, 按面积倒序, 最多 2000 条
         */
        virtual std::vector<models::Disturbance> findByProject(int projectId, std::optional<int> periodId = std::nullopt);

        /**
         * @brief 分页列出所有扰动
         * @param page 页码, 从 1 开始
         * @param pageSize 每页条数
         * @param orderBy 排序列名
         * @param ascending 是否升序
         * @return 当前页记录列表
         */
        std::vector<models::Disturbance> list(int page = 1, int pageSize = 20, const std::string& orderBy = "id",
                                              bool ascending = true) override;

        /**
         * @brief 新增扰动
         * @param disturbance 待插入记录; 几何以 ST_GeomFromText 写入 (SRID 4326)
         * @return 新记录 id; 失败返回 -1
         */
        int insert(const models::Disturbance& disturbance) override;

        /**
         * @brief 逐条批量新增扰动
         * @param disturbances 待插入记录列表
         * @return 成功插入的 id 列表 (失败的跳过)
         */
        virtual std::vector<int> batchInsert(const std::vector<models::Disturbance>& disturbances);

        /**
         * @brief 按 id 更新扰动类型、面积、周长、置信度
         * @param disturbance 记录 (须含 id)
         * @return 更新成功返回 true
         */
        bool update(const models::Disturbance& disturbance) override;

        /**
         * @brief 按 id 删除扰动
         * @param id 扰动主键
         * @return 删除成功返回 true
         */
        bool remove(int id) override;

        /**
         * @brief 扰动总数
         * @return 表内记录数
         */
        int count() override;

    private:
        /// @brief 行数据 -> 实体 (几何以 WKT 读出)
        models::Disturbance mapRowToDisturbance(const std::map<std::string, std::string>& row);
    };

}  // namespace soilwatercm::dao
