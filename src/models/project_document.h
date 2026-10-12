#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace soilwatercm::models
{

    /**
     * @brief 项目资料附件, 对应 project_documents 表
     *
     * 方案/批复/设计/监理/验收等文档的上传记录。
     */
    struct ProjectDocument
    {
        int id = 0;
        int project_id = 0;
        std::string doc_type = "other";  ///< 资料类别: scheme/approval/design/supervision/acceptance/other
        std::optional<std::string> title;
        std::string name;  ///< 原文件名
        std::string file_path;
        std::optional<double> file_size_kb;  ///< 文件大小 (KB)
        std::chrono::system_clock::time_point uploaded_at;
    };

}  // namespace soilwatercm::models
