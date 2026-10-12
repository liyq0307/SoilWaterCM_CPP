#pragma once

#include "../models/project.h"
#include "dao_base.h"

namespace soilwatercm::dao
{

    /**
     * @brief 项目数据访问对象
     *
     * 对应 projects 表: 项目基本信息 (编号、名称、责任面积几何、中心点、状态)。
     */
    class ProjectDao : public DaoBase<models::Project>
    {
    public:
        /**
         * @brief 构造, 注入数据库仓库
         * @param repo PostgreSQL 访问仓库
         */
        explicit ProjectDao(std::shared_ptr<repository::PostgresRepository> repo);

        /// @brief 虚析构, 支持多态删除
        ~ProjectDao() override = default;

        /**
         * @brief 按 id 查项目
         * @param id 项目主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::Project> findById(int id) override;

        /**
         * @brief 按项目编号查项目
         * @param code 项目编号
         * @return 记录; 不存在返回 nullopt
         */
        virtual std::optional<models::Project> findByCode(const std::string& code);

        /**
         * @brief 分页列出所有项目
         * @param page 页码, 从 1 开始
         * @param pageSize 每页条数
         * @param orderBy 排序列名
         * @param ascending 是否升序
         * @return 当前页记录列表
         */
        std::vector<models::Project> list(int page = 1, int pageSize = 20, const std::string& orderBy = "id",
                                          bool ascending = true) override;

        /**
         * @brief 按状态查项目
         * @param status 项目状态
         * @return 匹配记录列表
         */
        std::vector<models::Project> findByStatus(models::ProjectStatus status);

        /**
         * @brief 新增项目
         * @param project 待插入项目; 责任面积以 ST_GeomFromText 写入 (SRID 4326)
         * @return 新记录 id; 失败返回 -1
         */
        int insert(const models::Project& project) override;

        /**
         * @brief 更新项目名称、责任面积、中心点、状态
         * @param project 项目 (须含 id); 责任面积以 ST_GeomFromText 写入 (SRID 4326)
         * @return 更新成功返回 true
         */
        bool update(const models::Project& project) override;

        /**
         * @brief 删除项目, 并级联删除其期次、影像、异常、报告
         * @param id 项目主键
         * @return 删除成功返回 true
         */
        bool remove(int id) override;

        /**
         * @brief 项目总数
         * @return projects 表记录数
         */
        int count() override;

        /**
         * @brief 项目编号是否已存在
         * @param code 项目编号
         * @return 已存在返回 true
         */
        virtual bool existsByCode(const std::string& code);

        /**
         * @brief 按编号或名称模糊搜索项目
         * @param keyword 关键词, 对 code 或 name 做 LIKE 匹配
         * @return 匹配记录列表
         */
        std::vector<models::Project> search(const std::string& keyword);

    private:
        /// @brief 行数据 -> 项目实体 (责任面积以 WKT 读出)
        models::Project mapRowToProject(const std::map<std::string, std::string>& row);
    };

}  // namespace soilwatercm::dao
