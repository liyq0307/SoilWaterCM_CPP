#pragma once
#include "../models/report.h"
#include "dao_base.h"
namespace soilwatercm::dao
{
    /**
     * @brief 水保报告数据访问对象
     *
     * 对应 reports 表: 水保报告 (类型、标题、docx/pdf 路径、归档编号与日期)。
     * 另提供审批意见读写与报告预览所需的统计/元信息查询。
     */
    class ReportDao : public DaoBase<models::Report>
    {
    public:
        /**
         * @brief 构造, 注入数据库仓库
         * @param repo PostgreSQL 访问仓库
         */
        explicit ReportDao(std::shared_ptr<repository::PostgresRepository> repo);

        /**
         * @brief 按 id 查报告
         * @param id 报告主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::Report> findById(int id) override;

        /**
         * @brief 按项目查报告
         * @param projectId 项目 id
         * @return 匹配记录, 按创建时间倒序
         */
        std::vector<models::Report> findByProjectId(int projectId);

        /**
         * @brief 分页列出所有报告
         * @param page 页码, 从 1 开始
         * @param pageSize 每页条数
         * @param orderBy 排序列名
         * @param ascending 是否升序
         * @return 当前页记录列表
         */
        std::vector<models::Report> list(int page = 1, int pageSize = 20, const std::string& orderBy = "id",
                                         bool ascending = true) override;

        /**
         * @brief 新增报告
         * @param r 待插入记录
         * @return 新记录 id; 失败返回 -1
         */
        int insert(const models::Report& r) override;

        /**
         * @brief 按 id 更新报告标题和文件路径
         * @param r 记录 (须含 id)
         * @return 更新成功返回 true
         */
        bool update(const models::Report& r) override;

        /**
         * @brief 按 id 删除报告
         * @param id 报告主键
         * @return 删除成功返回 true
         */
        bool remove(int id) override;

        /**
         * @brief 报告总数
         * @return 表内记录数
         */
        int count() override;

        /**
         * @brief 标记报告已归档, 写入归档编号和当天日期
         * @param id 报告主键
         * @param filingNumber 归档编号
         * @return 更新成功返回 true
         */
        bool markAsFiled(int id, const std::string& filingNumber);

        /**
         * @brief 查 tasks 表拿生成任务的进度和错误信息
         * @param taskId 任务 id
         * @return 含 status/progress/message 的映射; 任务不存在返回空映射
         */
        std::map<std::string, std::string> getTaskProgress(int taskId);

        /**
         * @brief 读报告的审批意见文本
         * @param id 报告主键
         * @return 意见文本; 报告不存在返回 nullopt
         */
        std::optional<std::string> getOpinion(int id);

        /**
         * @brief 写报告的审批意见文本
         * @param id 报告主键
         * @param opinionText 意见文本
         * @return 更新成功返回 true
         */
        bool updateOpinion(int id, const std::string& opinionText);

        /**
         * @brief 汇总项目/期次的统计数, 供报告预览
         * @param projectId 项目 id
         * @param periodId 期次 id; 小于等于 0 则汇总项目全期次
         * @return 扰动数量面积、异常数量、平均 FVC、平均侵蚀模数
         */
        std::map<std::string, double> previewStats(int projectId, int periodId);

        /**
         * @brief 查项目名/编号和期次信息, 供报告预览
         * @param projectId 项目 id
         * @param periodId 期次 id; 小于等于 0 则不查期次
         * @return 含项目名/编号、期次编号和起止范围的映射
         */
        std::map<std::string, std::string> previewMeta(int projectId, int periodId);

    private:
        /// @brief 行数据 -> 实体
        models::Report mapRowToReport(const std::map<std::string, std::string>& row);
    };
}  // namespace soilwatercm::dao
