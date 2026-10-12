#pragma once
#include "../models/evaluation.h"
#include "dao_base.h"
namespace soilwatercm::dao
{
    /**
     * @brief 水保成效评价数据访问对象
     *
     * 对应 evaluations 表: 每期次的水保成效评价 (合规得分/等级、风险得分/等级)。
     */
    class EvaluationDao : public DaoBase<models::Evaluation>
    {
    public:
        /**
         * @brief 构造, 注入数据库仓库
         * @param repo PostgreSQL 访问仓库
         */
        explicit EvaluationDao(std::shared_ptr<repository::PostgresRepository> repo);

        /**
         * @brief 按 id 查评价
         * @param id 评价主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::Evaluation> findById(int id) override;

        /**
         * @brief 按期次查评价 (每期次一条)
         * @param periodId 期次 id
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::Evaluation> findByPeriodId(int periodId);

        /**
         * @brief 分页列出所有评价
         * @param page 页码, 从 1 开始
         * @param pageSize 每页条数
         * @param orderBy 排序列名
         * @param ascending 是否升序
         * @return 当前页记录列表
         */
        std::vector<models::Evaluation> list(int page = 1, int pageSize = 20, const std::string& orderBy = "id",
                                             bool ascending = true) override;

        /**
         * @brief 新增评价
         * @param e 待插入记录
         * @return 新记录 id; 失败返回 -1
         */
        int insert(const models::Evaluation& e) override;

        /**
         * @brief 按 id 更新得分和等级
         * @param e 记录 (须含 id)
         * @return 更新成功返回 true
         */
        bool update(const models::Evaluation& e) override;

        /**
         * @brief 按期次写入: 已有则更新, 没有则新增
         * @param e 评价记录 (以 period_id 判定)
         * @return 写入成功返回 true
         */
        bool upsert(const models::Evaluation& e);

        /**
         * @brief 按 id 删除评价
         * @param id 评价主键
         * @return 删除成功返回 true
         */
        bool remove(int id) override;

        /**
         * @brief 评价总数
         * @return 表内记录数
         */
        int count() override;

    private:
        /// @brief 行数据 -> 实体
        models::Evaluation mapRowToEvaluation(const std::map<std::string, std::string>& row);
    };
}  // namespace soilwatercm::dao
