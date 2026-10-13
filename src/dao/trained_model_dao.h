#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../models/trained_model.h"
#include "dao_base.h"

namespace soilwatercm::dao
{
    /**
     * @brief 训练模型注册数据访问对象
     *
     * 对应 trained_models 表: 模型文件与指标快照的注册表,
     * 独立于任务记录维护模型生命周期。
     */
    class TrainedModelDao : public DaoBase<models::TrainedModel>
    {
    public:
        explicit TrainedModelDao(std::shared_ptr<repository::PostgresRepository> repo);

        std::optional<models::TrainedModel> findById(int id) override;
        std::vector<models::TrainedModel> list(int page = 1, int pageSize = 20, const std::string& orderBy = "id",
                                               bool ascending = true) override;
        int insert(const models::TrainedModel& tm) override;
        bool update(const models::TrainedModel& tm) override;
        bool remove(int id) override;
        int count() override;

        /**
         * @brief 按项目查模型注册, 默认只要 available 状态
         * @param projectId 项目 id
         * @param includeArchived 是否含归档模型
         * @return 按创建时间倒序的注册列表
         */
        std::vector<models::TrainedModel> findByProject(int projectId, bool includeArchived = false);

        /**
         * @brief 查项目最新可用模型
         * @param projectId 项目 id
         * @return 最新一条 available 注册; 不存在返回 nullopt
         */
        std::optional<models::TrainedModel> latestByProject(int projectId);

        /**
         * @brief 归档模型 (status -> archived)
         * @param tm 记录 (须含 id)
         * @return 更新成功返回 true
         */
        bool archive(const models::TrainedModel& tm);

    private:
        /// @brief 行数据 -> 实体
        models::TrainedModel mapRow(const std::map<std::string, std::string>& row);
    };
}  // namespace soilwatercm::dao
