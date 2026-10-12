#pragma once

#include "../models/monitoring_period.h"
#include "dao_base.h"

namespace soilwatercm::dao
{

    /**
     * @brief 监测期次数据访问对象
     *
     * 对应 monitoring_periods 表: 项目的监测期次 (编号、类型、起止日期、状态)。
     */
    class MonitoringPeriodDao : public DaoBase<models::MonitoringPeriod>
    {
    public:
        /**
         * @brief 构造, 注入数据库仓库
         * @param repo PostgreSQL 访问仓库
         */
        explicit MonitoringPeriodDao(std::shared_ptr<repository::PostgresRepository> repo);

        /**
         * @brief 按 id 查期次
         * @param id 期次主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::MonitoringPeriod> findById(int id) override;

        /**
         * @brief 按项目查所有期次
         * @param projectId 项目 id
         * @return 匹配记录, 按开始日期倒序
         */
        virtual std::vector<models::MonitoringPeriod> findByProjectId(int projectId);

        /**
         * @brief 分页列出所有期次
         * @param page 页码, 从 1 开始
         * @param pageSize 每页条数
         * @param orderBy 排序列名
         * @param ascending 是否升序
         * @return 当前页记录列表
         */
        std::vector<models::MonitoringPeriod> list(int page = 1, int pageSize = 20, const std::string& orderBy = "id",
                                                   bool ascending = true) override;

        /**
         * @brief 新增期次
         * @param period 待插入记录
         * @return 新记录 id; 失败返回 -1
         */
        int insert(const models::MonitoringPeriod& period) override;

        /**
         * @brief 按 id 更新期次编号、类型、起止日期、状态
         * @param period 记录 (须含 id)
         * @return 更新成功返回 true
         */
        bool update(const models::MonitoringPeriod& period) override;

        /**
         * @brief 按 id 删除期次
         * @param id 期次主键
         * @return 删除成功返回 true
         */
        bool remove(int id) override;

        /**
         * @brief 期次总数
         * @return 表内记录数
         */
        int count() override;

    private:
        /// @brief 行数据 -> 实体
        models::MonitoringPeriod mapRowToPeriod(const std::map<std::string, std::string>& row);
    };

}  // namespace soilwatercm::dao
