#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "common/enums.h"
#include <nlohmann/json.hpp>

namespace soilwatercm::models
{

    /**
     * @brief 监测报告, 对应 reports 表
     *
     * 季度/年度/总结报告。记录生成的 docx/pdf 路径与向水行政主管部门
     * 备案的文号、日期。
     */
    struct Report
    {
        int id = 0;
        ReportType type = ReportType::Quarterly;
        std::string title;
        std::string docx_path;
        std::string pdf_path;
        bool is_filed = false;  ///< 是否已报送水行政主管部门备案
        std::optional<std::string> filing_number;  ///< 备案文号
        std::optional<std::chrono::system_clock::time_point> filing_date;
        int project_id = 0;
        std::vector<int> period_ids;  ///< 报告覆盖的监测期
        std::chrono::system_clock::time_point created_at;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Report, id, type, title, docx_path, pdf_path, is_filed, filing_number,
                                       filing_date, project_id, period_ids)
    };

}  // namespace soilwatercm::models
