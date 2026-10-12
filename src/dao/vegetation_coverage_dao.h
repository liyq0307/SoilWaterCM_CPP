#pragma once
#include "../models/vegetation_coverage.h"
#include "dao_base.h"
namespace soilwatercm::dao
{
    /**
     * @brief 植被覆盖度数据访问对象
     *
     * 对应 vegetation_coverages 表: 每期次的植被覆盖度结果 (FVC 栅格路径、
     * 平均 FVC、各等级覆盖面积)。
     */
    class VegetationCoverageDao : public DaoBase<models::VegetationCoverage>
    {
    public:
        /**
         * @brief 构造, 注入数据库仓库
         * @param repo PostgreSQL 访问仓库
         */
        explicit VegetationCoverageDao(std::shared_ptr<repository::PostgresRepository> repo);

        /**
         * @brief 按 id 查植被覆盖度结果
         * @param id 结果主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::VegetationCoverage> findById(int id) override;

        /**
         * @brief 按期次查植被覆盖度结果 (每期次一条)
         * @param periodId 期次 id
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::VegetationCoverage> findByPeriodId(int periodId);

        /**
         * @brief 分页列出所有结果
         * @param page 页码, 从 1 开始
         * @param pageSize 每页条数
         * @param orderBy 排序列名
         * @param ascending 是否升序
         * @return 当前页记录列表
         */
        std::vector<models::VegetationCoverage> list(int page = 1, int pageSize = 20, const std::string& orderBy = "id",
                                                     bool ascending = true) override;

        /**
         * @brief 新增结果
         * @param vc 待插入记录
         * @return 新记录 id; 失败返回 -1
         */
        int insert(const models::VegetationCoverage& vc) override;

        /**
         * @brief 按 id 更新栅格路径和平均 FVC
         * @param vc 记录 (须含 id)
         * @return 更新成功返回 true
         */
        bool update(const models::VegetationCoverage& vc) override;

        /**
         * @brief 按 id 删除结果
         * @param id 结果主键
         * @return 删除成功返回 true
         */
        bool remove(int id) override;

        /**
         * @brief 结果总数
         * @return 表内记录数
         */
        int count() override;

    private:
        /// @brief 行数据 -> 实体
        models::VegetationCoverage mapRowToVC(const std::map<std::string, std::string>& row);
    };
}  // namespace soilwatercm::dao
