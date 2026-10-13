#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../models/terrain_result.h"
#include "dao_base.h"

namespace soilwatercm::dao
{
    /**
     * @brief 地形/专项分析成果数据访问对象
     *
     * 对应 terrain_results 表: 地形指数、淹没、视域、InSAR 形变等
     * 单值栅格成果, analysis 区分类型。
     */
    class TerrainResultDao : public DaoBase<models::TerrainResult>
    {
    public:
        explicit TerrainResultDao(std::shared_ptr<repository::PostgresRepository> repo);

        std::optional<models::TerrainResult> findById(int id) override;
        std::vector<models::TerrainResult> list(int page = 1, int pageSize = 20, const std::string& orderBy = "id",
                                                bool ascending = true) override;
        int insert(const models::TerrainResult& tr) override;
        bool update(const models::TerrainResult& tr) override;
        bool remove(int id) override;
        int count() override;

        /**
         * @brief 按项目查成果, 可按 analysis 过滤
         * @param projectId 项目 id
         * @param analysis 分析类型; 为空则不过滤
         * @return 按创建时间倒序的成果列表
         */
        std::vector<models::TerrainResult> listByProject(int projectId, const std::string& analysis = "");

        /**
         * @brief 查项目某类分析的最新成果
         * @param projectId 项目 id
         * @param analysis 分析类型 (inundation/viewshed/insar 等)
         * @return 最新一条; 不存在返回 nullopt
         */
        std::optional<models::TerrainResult> latestByProjectAndAnalysis(int projectId, const std::string& analysis);

    private:
        models::TerrainResult mapRow(const std::map<std::string, std::string>& row);
    };
}  // namespace soilwatercm::dao
