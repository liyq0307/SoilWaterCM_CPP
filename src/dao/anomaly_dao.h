#pragma once
#include "../models/anomaly.h"
#include "dao_base.h"
namespace soilwatercm::dao
{
    /**
     * @brief 水保异常事件数据访问对象
     *
     * 对应 anomalies 表: 水保异常事件 (类型、位置几何、严重度、描述、处置状态、
     * 责任人)。
     */
    class AnomalyDao : public DaoBase<models::Anomaly>
    {
    public:
        /**
         * @brief 构造, 注入数据库仓库
         * @param repo PostgreSQL 访问仓库
         */
        explicit AnomalyDao(std::shared_ptr<repository::PostgresRepository> repo);

        /**
         * @brief 按 id 查异常
         * @param id 异常主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::Anomaly> findById(int id) override;

        /**
         * @brief 按项目查异常
         * @param projectId 项目 id
         * @return 匹配记录, 按发现时间倒序
         */
        std::vector<models::Anomaly> findByProjectId(int projectId);

        /**
         * @brief 分页列出所有异常
         * @param page 页码, 从 1 开始
         * @param pageSize 每页条数
         * @param orderBy 排序列名
         * @param ascending 是否升序
         * @return 当前页记录列表
         */
        std::vector<models::Anomaly> list(int page = 1, int pageSize = 20, const std::string& orderBy = "id",
                                          bool ascending = true) override;

        /**
         * @brief 新增异常
         * @param a 待插入记录; 位置以 ST_GeomFromText 写入 (SRID 4326)
         * @return 新记录 id; 失败返回 -1
         */
        int insert(const models::Anomaly& a) override;

        /**
         * @brief 按 id 更新处置状态和描述
         * @param a 记录 (须含 id)
         * @return 更新成功返回 true
         */
        bool update(const models::Anomaly& a) override;

        /**
         * @brief 按 id 删除异常
         * @param id 异常主键
         * @return 删除成功返回 true
         */
        bool remove(int id) override;

        /**
         * @brief 异常总数
         * @return 表内记录数
         */
        int count() override;

        /**
         * @brief 按处置状态查异常
         * @param status 处置状态
         * @return 匹配记录, 按发现时间倒序
         */
        std::vector<models::Anomaly> findByStatus(models::DisposalStatus status);

        /**
         * @brief 指派责任人
         * @param id 异常主键
         * @param assigneeId 责任人用户 id (以文本写入 assignee 列)
         * @return 更新成功返回 true
         */
        bool updateAssignee(int id, int assigneeId);

        /**
         * @brief 更新处置状态
         * @param id 异常主键
         * @param status 新处置状态
         * @return 更新成功返回 true
         */
        bool updateDisposalStatus(int id, models::DisposalStatus status);

        /**
         * @brief 更新处置详情 (状态、严重度、描述、措施、责任人、解决时间)
         * @param a 记录 (须含 id); 可选字段为空写 NULL
         * @return 更新成功返回 true
         */
        bool updateDetails(const models::Anomaly& a);

    private:
        // @brief 行数据 -> 实体 (几何以 WKT 读出)
        models::Anomaly mapRowToAnomaly(const std::map<std::string, std::string>& row);
    };
}  // namespace soilwatercm::dao
