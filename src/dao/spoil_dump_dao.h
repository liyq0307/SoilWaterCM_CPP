#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../models/spoil_dump.h"
#include "dao_base.h"

namespace soilwatercm::dao
{
    /**
     * @brief 弃渣场图斑数据访问对象
     *
     * 对应 spoil_dumps 表: 堆渣方量/堆高/稳定性/合规性。
     */
    class SpoilDumpDao : public DaoBase<models::SpoilDump>
    {
    public:
        explicit SpoilDumpDao(std::shared_ptr<repository::PostgresRepository> repo);

        std::optional<models::SpoilDump> findById(int id) override;
        std::vector<models::SpoilDump> list(int page = 1, int pageSize = 20, const std::string& orderBy = "id",
                                             bool ascending = true) override;
        int insert(const models::SpoilDump& sd) override;
        bool update(const models::SpoilDump& sd) override;
        bool remove(int id) override;
        int count() override;

        /**
         * @brief 按项目查弃渣场图斑, 可按期次/合规性/稳定性过滤
         * @return 按堆渣方量倒序的图斑列表
         */
        std::vector<models::SpoilDump> findByProject(int projectId, std::optional<int> periodId = std::nullopt,
                                                     const std::string& compliance = "",
                                                     const std::string& stabilityLevel = "");

    private:
        /// @brief 行数据 -> 实体
        models::SpoilDump mapRow(const std::map<std::string, std::string>& row);
    };
}  // namespace soilwatercm::dao
