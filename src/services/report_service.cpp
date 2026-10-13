#include "report_service.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

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

        // PDF 内容流文本转义: 去非 ASCII 字节 (避免中文字体缺失) 并转义 \ ( )
        std::string escapePdfAscii(const std::string& s)
        {
            std::string out;
            for (unsigned char c : s)
            {
                if (c == '\\')
                    out += "\\\\";
                else if (c == '(')
                    out += "\\(";
                else if (c == ')')
                    out += "\\)";
                else if (c >= 0x20 && c < 0x7F)
                    out += static_cast<char>(c);
            }
            return out;
        }

        // 由 PDF 文件名 stem 提取 ASCII 标题 (剔除非 ASCII), 空则返回空串
        std::string paramsTitleToAscii(const std::string& pdfPath)
        {
            std::string stem = std::filesystem::path(pdfPath).stem().string();
            std::string out;
            for (unsigned char c : stem)
                if (c >= 0x20 && c < 0x7F)
                    out += static_cast<char>(c);
            return out;
        }

        // ---- 最小 ZIP (stored, 不压缩) 写入器: 生成可被 Word 打开的真 DOCX ----

        // CRC32 (IEEE 802.3, 与 ZIP 规范一致), 查表法
        struct Crc32
        {
            static const uint32_t* table()
            {
                static uint32_t tbl[256] = {};
                static bool init = []
                {
                    for (uint32_t i = 0; i < 256; ++i)
                    {
                        uint32_t c = i;
                        for (int k = 0; k < 8; ++k)
                            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
                        tbl[i] = c;
                    }
                    return true;
                }();
                (void)init;
                return tbl;
            }
            static uint32_t of(const std::string& data)
            {
                const auto* t = table();
                uint32_t c = 0xFFFFFFFFu;
                for (unsigned char ch : data)
                    c = t[(c ^ ch) & 0xFF] ^ (c >> 8);
                return c ^ 0xFFFFFFFFu;
            }
        };

        struct ZipEntry
        {
            std::string name;
            std::string data;
        };

        // 追加小端 uint16/uint32 到输出流
        void putU16(std::ostream& out, uint16_t v)
        {
            char b[2] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF)};
            out.write(b, 2);
        }
        void putU32(std::ostream& out, uint32_t v)
        {
            char b[4] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF),
                         static_cast<char>((v >> 16) & 0xFF), static_cast<char>((v >> 24) & 0xFF)};
            out.write(b, 4);
        }

        // 写入 stored (无压缩) ZIP 文件; 每个 entry 保留原大小/方法 0
        void writeStoredZip(const std::string& path, const std::vector<ZipEntry>& entries)
        {
            std::ostringstream local;
            std::ostringstream central;
            uint32_t offset = 0;

            for (const auto& e : entries)
            {
                uint32_t crc = Crc32::of(e.data);
                uint16_t nameLen = static_cast<uint16_t>(e.name.size());
                uint32_t dataSize = static_cast<uint32_t>(e.data.size());

                // 本地文件头 (30 字节 + 文件名 + 数据)
                local << char(0x50) << char(0x4B) << char(0x03) << char(0x04); // 签名 PK\x03\x04
                putU16(local, 20);     // 解压所需版本
                putU16(local, 0);       // 通用标志
                putU16(local, 0);       // 方法 0 = stored
                putU16(local, 0);       // mod time
                putU16(local, 0x21);    // mod date (1980-01-01, 非 0 以避免部分解析器告警)
                putU32(local, crc);
                putU32(local, dataSize); // 压缩后 (stored 相同)
                putU32(local, dataSize); // 未压缩
                putU16(local, nameLen);
                putU16(local, 0);         // 额外字段长度
                local.write(e.name.data(), static_cast<std::streamsize>(nameLen));
                local.write(e.data.data(), static_cast<std::streamsize>(dataSize));

                // 中央目录记录 (46 字节 + 文件名)
                uint32_t centralOffset = offset;
                central << char(0x50) << char(0x4B) << char(0x01) << char(0x02); // 签名 PK\x01\x02
                putU16(central, 20); // 创建版本
                putU16(central, 20);  // 解压所需版本
                putU16(central, 0);  // 标志
                putU16(central, 0);  // 方法
                putU16(central, 0);  // time
                putU16(central, 0x21);
                putU32(central, crc);
                putU32(central, dataSize);
                putU32(central, dataSize);
                putU16(central, nameLen);
                putU16(central, 0); // 额外
                putU16(central, 0); // 注释
                putU16(central, 0); // disk
                putU16(central, 0); // 内部属性
                putU32(central, 0); // 外部属性
                putU32(central, centralOffset);
                central.write(e.name.data(), static_cast<std::streamsize>(nameLen));

                offset += 30 + nameLen + dataSize;
            }

            std::string localStr = local.str();
            std::string centralStr = central.str();
            uint32_t cdSize = static_cast<uint32_t>(centralStr.size());
            uint32_t cdOffset = static_cast<uint32_t>(localStr.size());

            std::ofstream ofs(path, std::ios::binary);
            ofs << localStr;
            ofs << centralStr;
            // 中央目录结束记录 (22 字节)
            ofs << char(0x50) << char(0x4B) << char(0x05) << char(0x06);
            putU16(ofs, 0); // disk
            putU16(ofs, 0); // cd 所在 disk
            putU16(ofs, static_cast<uint16_t>(entries.size()));
            putU16(ofs, static_cast<uint16_t>(entries.size()));
            putU32(ofs, cdSize);
            putU32(ofs, cdOffset);
            putU16(ofs, 0); // 注释长度
        }

        // 由 document.xml 组装完整 DOCX (含 Content_Types / 关系 / 正文)
        std::vector<ZipEntry> buildDocxParts(const std::string& documentXml)
        {
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
            return {
                {"[Content_Types].xml", contentTypes},
                {"_rels/.rels", rels},
                {"word/_rels/document.xml.rels", wordRels},
                {"word/document.xml", documentXml},
            };
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

        // DOCX 即 ZIP 包: 生成真 OOXML 容器 (Content_Types + 关系 + 正文), Word 可打开
        auto parts = buildDocxParts(documentXml);
        writeStoredZip(outputPath, parts);

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

        // 生成有效单页 PDF (ASCII 标题/时间戳, 中文字体不嵌入, 完整正文见 DOCX)
        std::string titleAscii = "SoilWaterCM Report";
        {
            std::string slug = paramsTitleToAscii(pdfPath);
            if (!slug.empty())
                titleAscii = slug;
        }
        std::string genAt = currentTimeStr();

        std::string contentStream = "BT /F1 18 Tf 72 720 Td (" + escapePdfAscii(titleAscii) + ") Tj ET\n"
                                    "BT /F1 10 Tf 72 700 Td (Generated " + escapePdfAscii(genAt) + ") Tj ET\n"
                                    "BT /F1 10 Tf 72 680 Td (Full report content in the accompanying .docx) Tj ET";

        std::vector<std::string> objects = {
            "<< /Type /Catalog /Pages 2 0 R >>",
            "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
            "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>",
            "<< /Length " + std::to_string(contentStream.size()) + " >>\nstream\n" + contentStream + "\nendstream",
            "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
        };

        std::ostringstream pdf;
        pdf << "%PDF-1.4\n";
        std::vector<uint32_t> offsets;
        for (size_t i = 0; i < objects.size(); ++i)
        {
            offsets.push_back(static_cast<uint32_t>(pdf.str().size()));
            pdf << (i + 1) << " 0 obj\n" << objects[i] << "\nendobj\n";
        }
        uint32_t xrefPos = static_cast<uint32_t>(pdf.str().size());
        pdf << "xref\n0 " << (objects.size() + 1) << "\n";
        pdf << "0000000000 65535 f \n";
        for (auto off : offsets)
        {
            char buf[12];
            std::snprintf(buf, sizeof(buf), "%010u", off);
            pdf << buf << " 00000 n \n";
        }
        pdf << "trailer\n<< /Size " << (objects.size() + 1) << " /Root 1 0 R >>\nstartxref\n" << xrefPos << "\n%%EOF";

        std::ofstream pdfFile(fullPdfPath, std::ios::binary);
        pdfFile << pdf.str();
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
