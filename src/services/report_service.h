#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../dao/report_dao.h"
#include "../infrastructure/file_manager.h"
#include "../models/report.h"

namespace soilwatercm::services
{

    /** @brief 报告生成入参 */
    struct ReportGenerateParams
    {
        int project_id;                   ///< 项目 id
        std::vector<int> period_ids;      ///< 参与汇总的期次 id 列表
        models::ReportType type;          ///< 报告类型
        std::string title;                ///< 报告标题 (同时作为输出文件名)
        std::string template_path;        ///< 模板路径
    };

    /**
     * @brief 监测报告生成服务
     *
     * 汇总数据渲染 DOCX 并尝试转 PDF (LibreOffice 失败则写占位 PDF),
     * 管理报告记录、归档编号、审批意见与预览数据。
     */
    class ReportService
    {
    public:
        /**
         * @brief 构造, 注入报告 DAO 与文件管理器
         * @param reportDao 报告 DAO
         * @param fileManager 文件管理器 (输出目录)
         */
        explicit ReportService(std::shared_ptr<dao::ReportDao> reportDao,
                               std::shared_ptr<infrastructure::FileManager> fileManager);

        ~ReportService() = default;

        /**
         * @brief 生成报告
         *
         * 汇总数据 -> DOCX -> PDF -> 入库返回报告记录; PDF 转换失败不影响生成。
         * @param params 生成入参
         * @return 含新 id 的报告记录
         * @throw ProcessingException DOCX 文件创建失败时
         */
        models::Report generateReport(const ReportGenerateParams& params);

        /**
         * @brief 收集报告数据
         * @param projectId 项目 id
         * @param periodIds 期次 id 列表
         * @return 数据项 map; 当前为简化实现, 仅填项目/期次/类型/时间等基本信息
         */
        std::map<std::string, std::string> collectData(int projectId, const std::vector<int>& periodIds);

        /**
         * @brief 按固定章节结构生成 DOCX (概况/方法/结果/评价/建议)
         * @param data 报告数据项
         * @param templatePath 模板路径 (当前未使用)
         * @param outputPath 输出路径
         * @return 输出路径
         * @throw ProcessingException 文件创建失败时
         */
        std::string generateDocx(const std::map<std::string, std::string>& data, const std::string& templatePath,
                                 const std::string& outputPath);

        /**
         * @brief LibreOffice 转 PDF
         * @param docxPath DOCX 路径
         * @param pdfPath 输出 PDF 路径
         * @return PDF 路径; 转换失败时写一个占位 PDF 文件
         */
        std::string convertToPdf(const std::string& docxPath, const std::string& pdfPath);

        /**
         * @brief 标记报告已归档并记录归档编号
         * @param reportId 报告主键
         * @param filingNumber 归档编号
         * @return 更新成功返回 true
         */
        bool markAsFiled(int reportId, const std::string& filingNumber);

        /**
         * @brief 查询生成任务进度 (读 tasks 表)
         * @param taskId 任务 id
         * @return 进度信息 map
         */
        std::map<std::string, std::string> getGenerateProgress(int taskId);

        /**
         * @brief 报告预览统计数据
         * @param projectId 项目 id
         * @param periodId 期次 id
         * @return 统计项 map
         */
        std::map<std::string, double> getPreviewData(int projectId, int periodId);

        /**
         * @brief 报告预览元信息
         * @param projectId 项目 id
         * @param periodId 期次 id
         * @return 元信息 map
         */
        std::map<std::string, std::string> getPreviewMeta(int projectId, int periodId);

        /**
         * @brief 读取审批意见
         * @param reportId 报告主键
         * @return 意见文本; 无意见返回 nullopt
         */
        std::optional<std::string> getOpinion(int reportId);

        /**
         * @brief 保存审批意见 (不做异步重新生成)
         * @param reportId 报告主键
         * @param opinionText 意见文本
         * @return 保存成功返回 true
         */
        bool saveOpinion(int reportId, const std::string& opinionText);

        /**
         * @brief 删除报告 (删记录 + docx/pdf 物理文件)
         * @param reportId 报告主键
         * @return 删除成功返回 true
         */
        bool deleteReport(int reportId);

        /**
         * @brief 按 id 查报告
         * @param id 报告主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::Report> getReportById(int id);

        /**
         * @brief 按项目查报告列表
         * @param projectId 项目 id
         * @return 报告列表
         */
        std::vector<models::Report> getReportsByProject(int projectId);

    private:
        std::shared_ptr<dao::ReportDao> reportDao_;
        std::shared_ptr<infrastructure::FileManager> fileManager_;
    };

}  // namespace soilwatercm::services
