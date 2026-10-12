#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../models/indicator.h"
#include "dao_base.h"

namespace soilwatercm::dao
{

    /**
     * @brief 评价指标数据访问对象
     *
     * 管理两张表: indicators (评价指标定义: 编码、维度、权重、阈值、方向) 和
     * indicator_values (指标实测值)。
     */
    class IndicatorDao : public DaoBase<models::Indicator>
    {
    public:
        /**
         * @brief 构造, 注入数据库仓库
         * @param repo PostgreSQL 访问仓库
         */
        explicit IndicatorDao(std::shared_ptr<repository::PostgresRepository> repo);

        // ---- Indicator ----

        /**
         * @brief 按 id 查指标
         * @param id 指标主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::Indicator> findById(int id) override;

        /**
         * @brief 分页列出所有指标
         * @param page 页码, 从 1 开始
         * @param pageSize 每页条数
         * @param orderBy 排序列名
         * @param ascending 是否升序
         * @return 当前页记录列表
         */
        std::vector<models::Indicator> list(int page = 1, int pageSize = 20, const std::string& orderBy = "id",
                                            bool ascending = true) override;

        /**
         * @brief 新增指标
         * @param indicator 待插入指标定义
         * @return 新记录 id; 失败返回 -1
         */
        int insert(const models::Indicator& indicator) override;

        /**
         * @brief 按 id 更新指标定义全字段
         * @param indicator 指标 (须含 id)
         * @return 更新成功返回 true
         */
        bool update(const models::Indicator& indicator) override;

        /**
         * @brief 按 id 删除指标
         * @param id 指标主键
         * @return 删除成功返回 true
         */
        bool remove(int id) override;

        /**
         * @brief 指标总数
         * @return 表内记录数
         */
        int count() override;

        /**
         * @brief 仅启用指标, 可按维度过滤 (对齐 Python GET /indicators)
         * @param dimension 维度; 为空则不过滤
         * @return 启用指标, 按维度和 id 排序
         */
        std::vector<models::Indicator> listActive(const std::string& dimension);

        /**
         * @brief 按指标编码查指标
         * @param code 指标编码
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::Indicator> findByCode(const std::string& code);

        /**
         * @brief 只更新指标权重
         * @param id 指标主键
         * @param weight 新权重
         * @return 更新成功返回 true
         */
        bool updateWeight(int id, double weight);

        /**
         * @brief 更新指标三档阈值 (好/警戒/差)
         * @param id 指标主键
         * @param good "好"阈值; 为空写 NULL
         * @param warn "警戒"阈值; 为空写 NULL
         * @param bad "差"阈值; 为空写 NULL
         * @return 更新成功返回 true
         */
        bool updateThresholds(int id, std::optional<double> good, std::optional<double> warn,
                              std::optional<double> bad);

        // ---- IndicatorValue ----

        /**
         * @brief 查项目的指标值
         * @param projectId 项目 id
         * @param periodId 期次 id; 为空则不过滤
         * @param indicatorId 指标 id; 为空则不过滤
         * @return 匹配记录, 按 id 排序
         */
        std::vector<models::IndicatorValue> listValues(int projectId, std::optional<int> periodId,
                                                       std::optional<int> indicatorId);

        /**
         * @brief 新增一条指标值
         * @param value 待插入记录
         * @return 新记录 id; 失败返回 -1
         */
        int insertValue(const models::IndicatorValue& value);

        /**
         * @brief 项目已计算的指标值条数
         * @param projectId 项目 id
         * @return 记录数
         */
        int countValues(int projectId);

    private:
        /// @brief 行数据 -> 指标定义实体
        models::Indicator mapRowToIndicator(const std::map<std::string, std::string>& row);

        /// @brief 行数据 -> 指标值实体
        models::IndicatorValue mapRowToValue(const std::map<std::string, std::string>& row);
    };

}  // namespace soilwatercm::dao
