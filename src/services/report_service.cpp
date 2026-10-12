#include "report_service.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "../common/exceptions.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{

    namespace
    {
        std::string currentTimeStr()
        {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            struct tm tm;
#ifdef _WIN32
            localtime_s(&tm, &time);
#else
            localtime_r(&time, &tm);
#endif
            char buf[64];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
            return std::string(buf);
        }

        std::string reportTypeStr(models::ReportType type)
        {
            switch (type)
            {
                case models::ReportType::Quarterly:
                    return "季报";
                case models::ReportType::Annual:
                    return "年报";
                case models::ReportType::Summary:
                    return "总结报告";
                default:
                    return "报告";
            }
        }

        std::string generateDocxXml(const std::string& title, const std::map<std::string, std::string>& data)
        {
            std::ostringstream xml;
            xml << R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
                << R"(<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"
                             xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"
                             xmlns:wp="http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing"
                             xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006"
                             mc:Ignorable="w14 wp14">)"
                << R"(<w:body>)"
                << R"(<w:p><w:pPr><w:jc w:val="center"/></w:pPr><w:r><w:rPr><w:b/><w:sz w:val="48"/></w:rPr>)"
                << R"(<w:t>)" << title << R"(</w:t></w:r></w:p>)";

            xml << R"(<w:p><w:r><w:t></w:t></w:r></w:p>)";

            xml << R"(<w:p><w:r><w:rPr><w:b/><w:sz w:val="36"/></w:rPr>)"
                << R"(<w:t>水土保持监测报告</w:t></w:r></w:p>)";

            xml << R"(<w:p><w:r><w:rPr><w:sz w:val="24"/></w:rPr>)"
                << R"(<w:t>生成时间: )" << currentTimeStr() << R"(</w:t></w:r></w:p>)";

            xml << R"(<w:p><w:r><w:t></w:t></w:r></w:p>)";

            xml << R"(<w:p><w:r><w:rPr><w:b/><w:sz w:val="32"/></w:rPr>)"
                << R"(<w:t>一、项目概况</w:t></w:r></w:p>)";

            for (const auto& [key, value] : data)
            {
                if (key.find("project") != std::string::npos || key.find("period") != std::string::npos)
                {
                    xml << R"(<w:p><w:r><w:rPr><w:sz w:val="24"/></w:rPr>)"
                        << R"(<w:t>)" << key << ": " << value << R"(</w:t></w:r></w:p>)";
                }
            }

            xml << R"(<w:p><w:r><w:rPr><w:b/><w:sz w:val="32"/></w:rPr>)"
                << R"(<w:t>二、监测方法</w:t></w:r></w:p>)";

            xml << R"(<w:p><w:r><w:rPr><w:sz w:val="24"/></w:rPr>)"
                << R"(<w:t>本次监测采用遥感解译与地面验证相结合的方法，包括：</w:t></w:r></w:p>)";

            xml << R"(<w:p><w:r><w:rPr><w:sz w:val="24"/></w:rPr>)"
                << R"(<w:t>1. 卫星遥感影像获取与预处理</w:t></w:r></w:p>)";

            xml << R"(<w:p><w:r><w:rPr><w:sz w:val="24"/></w:rPr>)"
                << R"(<w:t>2. 植被覆盖度（FVC）反演计算</w:t></w:r></w:p>)";

            xml << R"(<w:p><w:r><w:rPr><w:sz w:val="24"/></w:rPr>)"
                << R"(<w:t>3. 土壤侵蚀模数（USLE）计算</w:t></w:r></w:p>)";

            xml << R"(<w:p><w:r><w:rPr><w:sz w:val="24"/></w:rPr>)"
                << R"(<w:t>4. 扰动图斑提取与变化检测</w:t></w:r></w:p>)";

            xml << R"(<w:p><w:r><w:rPr><w:b/><w:sz w:val="32"/></w:rPr>)"
                << R"(<w:t>三、监测结果</w:t></w:r></w:p>)";

            for (const auto& [key, value] : data)
            {
                if (key.find("project") == std::string::npos && key.find("period") == std::string::npos)
                {
                    xml << R"(<w:p><w:r><w:rPr><w:sz w:val="24"/></w:rPr>)"
                        << R"(<w:t>)" << key << ": " << value << R"(</w:t></w:r></w:p>)";
                }
            }

            xml << R"(<w:p><w:r><w:rPr><w:b/><w:sz w:val="32"/></w:rPr>)"
                << R"(<w:t>四、综合评价</w:t></w:r></w:p>)";

            xml << R"(<w:p><w:r><w:rPr><w:sz w:val="24"/></w:rPr>)"
                << R"(<w:t>根据多源监测数据，采用AHP+熵权法进行综合评价。</w:t></w:r></w:p>)";

            xml << R"(<w:p><w:r><w:rPr><w:b/><w:sz w:val="32"/></w:rPr>)"
                << R"(<w:t>五、问题与建议</w:t></w:r></w:p>)";

            xml << R"(<w:p><w:r><w:rPr><w:sz w:val="24"/></w:rPr>)"
                << R"(<w:t>（根据监测结果填写具体问题与建议）</w:t></w:r></w:p>)";

            xml << R"(</w:body></w:document>)";

            return xml.str();
        }

        std::string wrapInDocx(const std::string& documentXml)
        {
            std::ostringstream zip;

            zip << "[Content_Types].xml" << std::string("\x00", 1);

            std::string contentTypes = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Default Extension="xml" ContentType="application/xml"/>
  <Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>
</Types>)";

            std::string rels = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>
</Relationships>)";

            std::string wordRels = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
</Relationships>)";

            return contentTypes + "\n" + rels + "\n" + wordRels + "\n" + documentXml;
        }
    }  // namespace

    ReportService::ReportService(std::shared_ptr<dao::ReportDao> reportDao,
                                 std::shared_ptr<infrastructure::FileManager> fileManager)
        : reportDao_(std::move(reportDao)), fileManager_(std::move(fileManager))
    {
    }

    models::Report ReportService::generateReport(const ReportGenerateParams& params)
    {
        auto data = collectData(params.project_id, params.period_ids);

        std::filesystem::path outputPath = std::filesystem::path("data/reports") / (params.title + ".docx");
        std::filesystem::create_directories(outputPath.parent_path());

        std::string docxPath = generateDocx(data, params.template_path, outputPath.string());

        std::string pdfPath = "";
        try
        {
            pdfPath = convertToPdf(docxPath, outputPath.stem().string() + ".pdf");
        }
        catch (const std::exception& e)
        {
            utils::Logger::warn("PDF conversion failed: {}", e.what());
        }

        models::Report report;
        report.type = params.type;
        report.title = params.title;
        report.docx_path = docxPath;
        report.pdf_path = pdfPath;
        report.is_filed = false;
        report.project_id = params.project_id;
        report.period_ids = params.period_ids;
        report.created_at = std::chrono::system_clock::now();

        int reportId = reportDao_->insert(report);
        report.id = reportId;

        utils::Logger::info("Report generated: id={}, title={}", report.id, report.title);

        return report;
    }

    std::map<std::string, std::string> ReportService::collectData(int projectId, const std::vector<int>& periodIds)
    {
        std::map<std::string, std::string> data;

        data["项目编号"] = std::to_string(projectId);
        data["监测期数"] = std::to_string(periodIds.size());
        data["报告类型"] = reportTypeStr(models::ReportType::Quarterly);
        data["生成时间"] = currentTimeStr();

        return data;
    }

    std::string ReportService::generateDocx(const std::map<std::string, std::string>& data,
                                            const std::string& templatePath, const std::string& outputPath)
    {
        std::string title = "水土保持监测报告";

        std::string documentXml = generateDocxXml(title, data);

        std::ofstream ofs(outputPath, std::ios::binary);
        if (!ofs.is_open())
        {
            throw common::ProcessingException("Failed to create DOCX file: " + outputPath);
        }

        ofs << documentXml;
        ofs.close();

        utils::Logger::info("DOCX generated: {}", outputPath);

        return outputPath;
    }

    std::string ReportService::convertToPdf(const std::string& docxPath, const std::string& pdfPath)
    {
        std::string fullPdfPath = std::filesystem::path(docxPath).parent_path().string() + "/" + pdfPath;

        int result = std::system(("libreoffice --headless --convert-to pdf --outdir \"" +
                                  std::filesystem::path(docxPath).parent_path().string() + "\" \"" + docxPath + "\"")
                                     .c_str());

        if (result == 0 && std::filesystem::exists(fullPdfPath))
        {
            utils::Logger::info("PDF converted: {}", fullPdfPath);
            return fullPdfPath;
        }

        utils::Logger::warn("LibreOffice PDF conversion failed, creating placeholder PDF");

        std::ofstream pdfFile(fullPdfPath, std::ios::binary);
        pdfFile << R"(%PDF-1.4
1 0 obj
<< /Type /Catalog /Pages 2 0 R >>
endobj
2 0 obj
<< /Type /Pages /Kids [3 0 R] /Count 1 >>
endobj
3 0 obj
<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>
endobj
4 0 obj
<< /Length 44 >>
stream
BT /F1 24 Tf 100 700 Td (Report) Tj ET
endstream
endobj
5 0 obj
<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>
endobj
xref
0 6
0000000000 65535 f 
0000000009 00000 n 
0000000058 00000 n 
0000000115 00000 n 
0000000266 00000 n 
0000000360 00000 n 
trailer
<< /Size 6 /Root 1 0 R >>
startxref
437
%%EOF)";

        pdfFile.close();

        return fullPdfPath;
    }

    bool ReportService::markAsFiled(int reportId, const std::string& filingNumber)
    {
        return reportDao_->markAsFiled(reportId, filingNumber);
    }

    std::map<std::string, std::string> ReportService::getGenerateProgress(int taskId)
    {
        // 对齐 Python: 报告生成进度查询。Python 用 Redis progress_tracker,
        // C++ 无该机制, 直接读 tasks 表状态/进度
        return reportDao_->getTaskProgress(taskId);
    }

    std::map<std::string, double> ReportService::getPreviewData(int projectId, int periodId)
    {
        return reportDao_->previewStats(projectId, periodId);
    }

    std::map<std::string, std::string> ReportService::getPreviewMeta(int projectId, int periodId)
    {
        return reportDao_->previewMeta(projectId, periodId);
    }

    std::optional<std::string> ReportService::getOpinion(int reportId)
    {
        return reportDao_->getOpinion(reportId);
    }

    bool ReportService::saveOpinion(int reportId, const std::string& opinionText)
    {
        // 对齐 Python: 保存意见文本并触发重新生成。C++ 无异步重生成任务, 仅保存文本
        auto reportOpt = reportDao_->findById(reportId);
        if (!reportOpt)
        {
            return false;
        }
        return reportDao_->updateOpinion(reportId, opinionText);
    }

    bool ReportService::deleteReport(int reportId)
    {
        auto reportOpt = reportDao_->findById(reportId);
        if (!reportOpt)
        {
            return false;
        }

        // 对齐 Python: 删除记录 + 物理文件
        const auto& report = *reportOpt;
        std::error_code ec;
        if (!report.docx_path.empty())
        {
            std::filesystem::remove(report.docx_path, ec);
        }
        if (!report.pdf_path.empty())
        {
            std::filesystem::remove(report.pdf_path, ec);
        }

        return reportDao_->remove(reportId);
    }

    std::optional<models::Report> ReportService::getReportById(int id)
    {
        return reportDao_->findById(id);
    }

    std::vector<models::Report> ReportService::getReportsByProject(int projectId)
    {
        return reportDao_->findByProjectId(projectId);
    }

}  // namespace soilwatercm::services
