#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "external_tool_interface.h"
#include "process_manager.h"

using namespace soilwatercm::infrastructure::external_tools;

namespace soilwatercm::infrastructure
{

    /// InSAR 工具类型
    enum class InsarTool
    {
        ISCE2,
        SNAP,
        NONE
    };

    /// 单个工具的探测结果
    struct ToolDetectionResult
    {
        std::string name;
        bool available;
        std::string version;
        std::string path;
    };

    /**
     * @brief 外部 GIS 工具探测
     *
     * 扫描本机 PATH / conda 环境, 探测各工具是否可用及其版本。
     */
    class ExternalToolDetector
    {
    public:
        ExternalToolDetector();

        ~ExternalToolDetector() = default;

        /// @brief 依次探测全部工具, 结果缓存到内部 map
        void detectAll();

        /// @brief 探测 GRASS GIS
        ToolDetectionResult detectGrass();

        /// @brief 探测 SAGA GIS
        ToolDetectionResult detectSaga();

        /// @brief 探测 OTB
        ToolDetectionResult detectOtb();

        /// @brief 探测 InSAR 工具, 优先 ISCE2 再 SNAP (仅非 Windows 走 conda 检查)
        InsarTool detectInsarTool();

        /// @brief 探测 MintPy
        ToolDetectionResult detectMintPy();

        /// @brief 探测 SNAPHU
        ToolDetectionResult detectSnaphu();

        /// @brief 探测 PyAPS3
        ToolDetectionResult detectPyaps3();

        /// @brief 探测 QGIS
        ToolDetectionResult detectQGIS();

        /// @brief 全部探测结果的可读报告
        std::string getDetectionReport();

        /**
         * @brief 校验工具版本是否达到最低要求
         * @param toolName 工具名 (须已探测过)
         * @param minVersion 最低版本
         * @return 可用且版本达标返回 true
         */
        bool validateVersion(const std::string& toolName, const std::string& minVersion);

        /// @brief 全部探测结果 (工具名 -> 结果)
        std::map<std::string, ToolDetectionResult> getAllResults() const;

    private:
        std::shared_ptr<spdlog::logger> logger_;
        std::unique_ptr<ProcessManager> process_manager_;

        std::map<std::string, ToolDetectionResult> detection_results_;

        bool grass_available_ = false;
        bool saga_available_ = false;
        bool otb_available_ = false;
        bool isce2_available_ = false;
        bool snap_available_ = false;
        bool mintpy_available_ = false;
        bool snaphu_available_ = false;
        bool pyaps3_available_ = false;
        bool qgis_available_ = false;

        std::string grass_version_;
        std::string saga_version_;
        std::string otb_version_;
        std::string isce2_version_;
        std::string snap_version_;
        std::string mintpy_version_;
        std::string snaphu_version_;
        std::string pyaps3_version_;
        std::string qgis_version_;

        bool checkTool(const std::string& command, std::string& version);

        bool checkCondaTool(const std::string& condaEnv, const std::string& command, std::string& version);

        bool compareVersions(const std::string& version1, const std::string& version2);

        std::string findExecutable(const std::string& name);

        bool checkQGISProcessing(const std::string& provider);
    };

}  // namespace soilwatercm::infrastructure
