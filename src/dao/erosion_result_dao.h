#pragma once
#include "../models/erosion_result.h"
#include "dao_base.h"
namespace soilwatercm::dao
{
    /**
     * @brief 土壤侵蚀结果数据访问对象
     *
     * 对应 erosion_results 表: 每期次的土壤侵蚀结果 (模数/分级栅格路径、
     * 平均模数、各级侵蚀面积)。
     */
    class ErosionResultDao : public DaoBase<models::ErosionResult>
    {
    public:
        /**
         * @brief 构造, 注入数据库仓库
         * @param repo PostgreSQL 访问仓库
         */
        explicit ErosionResultDao(std::shared_ptr<repository::PostgresRepository> repo);

        /**
         * @brief 按 id 查侵蚀结果
         * @param id 结果主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::ErosionResult> findById(int id) override;

        /**
         * @brief 按期次查侵蚀结果 (每期次一条)
         * @param periodId 期次 id
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::ErosionResult> findByPeriodId(int periodId);

        /**
         * @brief 分页列出所有侵蚀结果
         * @param page 页码, 从 1 开始
         * @param pageSize 每页条数
         * @param orderBy 排序列名
         * @param ascending 是否升序
         * @return 当前页记录列表
         */
        std::vector<models::ErosionResult> list(int page = 1, int pageSize = 20, const std::string& orderBy = "id",
                                                bool ascending = true) override;

        /**
         * @brief 新增侵蚀结果
         * @param er 待插入记录
         * @return 新记录 id; 失败返回 -1
         */
        int insert(const models::ErosionResult& er) override;

        /**
         * @brief 按 id 更新栅格路径和平均模数
         * @param er 记录 (须含 id)
         * @return 更新成功返回 true
         */
        bool update(const models::ErosionResult& er) override;

        /**
         * @brief 按 id 删除侵蚀结果
         * @param id 结果主键
         * @return 删除成功返回 true
         */
        bool remove(int id) override;

        /**
         * @brief 侵蚀结果总数
         * @return 表内记录数
         */
        int count() override;

    private:
        /// @brief 行数据 -> 实体
        models::ErosionResult mapRowToER(const std::map<std::string, std::string>& row);
    };
}  // namespace soilwatercm::dao
