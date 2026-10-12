#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../models/project_document.h"
#include "dao_base.h"

namespace soilwatercm::dao
{

    /**
     * @brief 项目附件资料数据访问对象
     *
     * 对应 project_documents 表: 项目附件资料 (类型、标题、原始文件名、存储路径、大小)。
     */
    class ProjectDocumentDao : public DaoBase<models::ProjectDocument>
    {
    public:
        /**
         * @brief 构造, 注入数据库仓库
         * @param repo PostgreSQL 访问仓库
         */
        explicit ProjectDocumentDao(std::shared_ptr<repository::PostgresRepository> repo);

        /**
         * @brief 按 id 查资料
         * @param id 资料主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::ProjectDocument> findById(int id) override;

        /**
         * @brief 分页列出所有资料
         * @param page 页码, 从 1 开始
         * @param pageSize 每页条数
         * @param orderBy 排序列名
         * @param ascending 是否升序
         * @return 当前页记录列表
         */
        std::vector<models::ProjectDocument> list(int page = 1, int pageSize = 20, const std::string& orderBy = "id",
                                                  bool ascending = true) override;

        /**
         * @brief 新增资料
         * @param doc 待插入记录
         * @return 新记录 id; 失败返回 -1
         */
        int insert(const models::ProjectDocument& doc) override;

        /**
         * @brief 按 id 更新资料全部字段
         * @param doc 记录 (须含 id)
         * @return 更新成功返回 true
         */
        bool update(const models::ProjectDocument& doc) override;

        /**
         * @brief 按 id 删除资料
         * @param id 资料主键
         * @return 删除成功返回 true
         */
        bool remove(int id) override;

        /**
         * @brief 资料总数
         * @return 表内记录数
         */
        int count() override;

        /**
         * @brief 按项目列出资料
         * @param projectId 项目 id
         * @return 匹配记录, 按上传时间倒序
         */
        std::vector<models::ProjectDocument> findByProject(int projectId);

    private:
        /// @brief 行数据 -> 实体
        models::ProjectDocument mapRow(const std::map<std::string, std::string>& row);
    };

}  // namespace soilwatercm::dao
