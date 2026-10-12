#include "external_tool_detector.h"

#include <algorithm>
#include <filesystem>
#include <sstream>

#include "utils/logger.h"

#pragma warning(disable : 4996)

namespace soilwatercm::infrastructure
{

    namespace fs = std::filesystem;

    ExternalToolDetector::ExternalToolDetector()
        : logger_(utils::Logger::getLogger("external_tool_detector")),
          process_manager_(std::make_unique<ProcessManager>())
    {
    }

    void ExternalToolDetector::detectAll()
    {
        logger_->info("Starting external tool detection...");

        detectQGIS();
        detectGrass();
        detectSaga();
        detectOtb();
        detectInsarTool();
        detectMintPy();
        detectSnaphu();
        detectPyaps3();

        logger_->info("External tool detection completed");
    }

    ToolDetectionResult ExternalToolDetector::detectGrass()
    {
        ToolDetectionResult result;
        result.name = "GRASS GIS";

        if (qgis_available_ && checkQGISProcessing("grass"))
        {
            result.available = true;
            result.version = grass_version_;
            result.path = "QGIS Processing";
            grass_available_ = true;
        }
        else
        {
            std::string grassPath = findExecutable("grass");
            if (!grassPath.empty())
            {
                std::string version;
                if (checkTool(grassPath + " --version", version))
                {
                    result.available = true;
                    result.version = version;
                    result.path = grassPath;
                    grass_available_ = true;
                    grass_version_ = version;
                }
            }
        }

        detection_results_["grass"] = result;
        return result;
    }

    ToolDetectionResult ExternalToolDetector::detectSaga()
    {
        ToolDetectionResult result;
        result.name = "SAGA GIS";

        if (qgis_available_ && checkQGISProcessing("saga"))
        {
            result.available = true;
            result.version = saga_version_;
            result.path = "QGIS Processing";
            saga_available_ = true;
        }
        else
        {
            std::string sagaPath = findExecutable("saga_cmd");
            if (!sagaPath.empty())
            {
                std::string version;
                if (checkTool(sagaPath + " --version", version))
                {
                    result.available = true;
                    result.version = version;
                    result.path = sagaPath;
                    saga_available_ = true;
                    saga_version_ = version;
                }
            }
        }

        detection_results_["saga"] = result;
        return result;
    }

    ToolDetectionResult ExternalToolDetector::detectOtb()
    {
        ToolDetectionResult result;
        result.name = "Orfeo ToolBox";

        std::string otbcliPath = findExecutable("otbcli");
        if (!otbcliPath.empty())
        {
            std::string version;
            if (checkTool(otbcliPath + " -help", version))
            {
                size_t pos = version.find("OTB version:");
                if (pos != std::string::npos)
                {
                    version = version.substr(pos + 12);
                    size_t end = version.find('\n');
                    if (end != std::string::npos)
                    {
                        version = version.substr(0, end);
                    }
                }
                result.available = true;
                result.version = version;
                result.path = otbcliPath;
                otb_available_ = true;
                otb_version_ = version;
            }
        }

        detection_results_["otb"] = result;
        return result;
    }

    InsarTool ExternalToolDetector::detectInsarTool()
    {
#ifndef _WIN32
        if (checkCondaTool("isce2", "python -c \"import isce; print(isce.__version__)\"", isce2_version_))
        {
            isce2_available_ = true;

            ToolDetectionResult result;
            result.name = "ISCE2";
            result.available = true;
            result.version = isce2_version_;
            result.path = "conda:isce2";
            detection_results_["isce2"] = result;

            return InsarTool::ISCE2;
        }
#endif

        std::string gptPath = findExecutable("gpt");
        if (!gptPath.empty())
        {
            std::string version;
            if (checkTool(gptPath + " -v", version))
            {
                snap_available_ = true;
                snap_version_ = version;

                ToolDetectionResult result;
                result.name = "SNAP";
                result.available = true;
                result.version = version;
                result.path = gptPath;
                detection_results_["snap"] = result;

                return InsarTool::SNAP;
            }
        }

        ToolDetectionResult result;
        result.name = "InSAR Tool";
        result.available = false;
        detection_results_["insar"] = result;

        return InsarTool::NONE;
    }

    ToolDetectionResult ExternalToolDetector::detectMintPy()
    {
        ToolDetectionResult result;
        result.name = "MintPy";

        if (checkCondaTool("mintpy", "python -c \"import mintpy; print(mintpy.__version__)\"", mintpy_version_))
        {
            result.available = true;
            result.version = mintpy_version_;
            result.path = "conda:mintpy";
            mintpy_available_ = true;
        }

        detection_results_["mintpy"] = result;
        return result;
    }

    ToolDetectionResult ExternalToolDetector::detectSnaphu()
    {
        ToolDetectionResult result;
        result.name = "SNAPHU";

        std::string snaphuPath = findExecutable("snaphu");
        if (!snaphuPath.empty())
        {
            std::string version;
            if (checkTool(snaphuPath + " -v", version))
            {
                result.available = true;
                result.version = version;
                result.path = snaphuPath;
                snaphu_available_ = true;
                snaphu_version_ = version;
            }
        }

        detection_results_["snaphu"] = result;
        return result;
    }

    ToolDetectionResult ExternalToolDetector::detectPyaps3()
    {
        ToolDetectionResult result;
        result.name = "pyaps3";

        if (checkCondaTool("mintpy", "python -c \"import pyaps3; print('available')\"", pyaps3_version_))
        {
            result.available = true;
            result.version = pyaps3_version_.empty() ? "available" : pyaps3_version_;
            result.path = "conda:mintpy";
            pyaps3_available_ = true;
        }

        detection_results_["pyaps3"] = result;
        return result;
    }

    ToolDetectionResult ExternalToolDetector::detectQGIS()
    {
        ToolDetectionResult result;
        result.name = "QGIS";

        std::string qgisPath = findExecutable("qgis_process");
        if (qgisPath.empty())
        {
            qgisPath = findExecutable("qgis");
        }

        if (!qgisPath.empty())
        {
            std::string version;
            if (checkTool(qgisPath + " --version", version))
            {
                result.available = true;
                result.version = version;
                result.path = qgisPath;
                qgis_available_ = true;
                qgis_version_ = version;
            }
        }

        detection_results_["qgis"] = result;
        return result;
    }

    std::string ExternalToolDetector::getDetectionReport()
    {
        std::ostringstream oss;
        oss << "\n=== External Tool Detection Report ===\n";

        for (const auto& [name, result] : detection_results_)
        {
            oss << result.name << ":\n";
            oss << "  Available: " << (result.available ? "Yes" : "No") << "\n";
            if (result.available)
            {
                oss << "  Version: " << result.version << "\n";
                oss << "  Path: " << result.path << "\n";
            }
            oss << "\n";
        }

        oss << "=== Summary ===\n";
        oss << "GRASS GIS: " << (grass_available_ ? "✓" : "✗") << "\n";
        oss << "SAGA GIS: " << (saga_available_ ? "✓" : "✗") << "\n";
        oss << "OTB: " << (otb_available_ ? "✓" : "✗") << "\n";
        oss << "ISCE2: " << (isce2_available_ ? "✓" : "✗") << "\n";
        oss << "SNAP: " << (snap_available_ ? "✓" : "✗") << "\n";
        oss << "MintPy: " << (mintpy_available_ ? "✓" : "✗") << "\n";
        oss << "SNAPHU: " << (snaphu_available_ ? "✓" : "✗") << "\n";
        oss << "pyaps3: " << (pyaps3_available_ ? "✓" : "✗") << "\n";
        oss << "QGIS: " << (qgis_available_ ? "✓" : "✗") << "\n";

        return oss.str();
    }

    bool ExternalToolDetector::validateVersion(const std::string& toolName, const std::string& minVersion)
    {
        auto it = detection_results_.find(toolName);
        if (it == detection_results_.end() || !it->second.available)
        {
            return false;
        }

        return compareVersions(it->second.version, minVersion);
    }

    std::map<std::string, ToolDetectionResult> ExternalToolDetector::getAllResults() const
    {
        return detection_results_;
    }

    bool ExternalToolDetector::checkTool(const std::string& command, std::string& version)
    {
        ProcessResult result = process_manager_->execute(command, std::vector<std::string>{}, "");

        if (result.exit_code == 0)
        {
            version = result.output;
            return true;
        }

        return false;
    }

    bool ExternalToolDetector::checkCondaTool(const std::string& condaEnv, const std::string& command,
                                              std::string& version)
    {
        std::string fullCommand = "conda run -n " + condaEnv + " " + command;
        return checkTool(fullCommand, version);
    }

    bool ExternalToolDetector::compareVersions(const std::string& version1, const std::string& version2)
    {
        std::vector<int> v1, v2;

        std::string v1Str = version1;
        std::string v2Str = version2;

        std::replace(v1Str.begin(), v1Str.end(), '.', ' ');
        std::replace(v2Str.begin(), v2Str.end(), '.', ' ');

        std::istringstream iss1(v1Str), iss2(v2Str);
        int num;
        while (iss1 >> num)
            v1.push_back(num);
        while (iss2 >> num)
            v2.push_back(num);

        size_t maxLen = std::max(v1.size(), v2.size());
        while (v1.size() < maxLen)
            v1.push_back(0);
        while (v2.size() < maxLen)
            v2.push_back(0);

        for (size_t i = 0; i < maxLen; ++i)
        {
            if (v1[i] < v2[i])
                return false;
            if (v1[i] > v2[i])
                return true;
        }

        return true;
    }

    std::string ExternalToolDetector::findExecutable(const std::string& name)
    {
#ifdef _WIN32
        std::vector<std::string> extensions = {".exe", ".bat", ".cmd"};
        for (const auto& ext : extensions)
        {
            std::string fullName = name + ext;

            const char* pathEnv = std::getenv("PATH");
            if (pathEnv)
            {
                std::string pathStr(pathEnv);
                std::istringstream iss(pathStr);
                std::string dir;

                while (std::getline(iss, dir, ';'))
                {
                    fs::path execPath = fs::path(dir) / fullName;
                    if (fs::exists(execPath))
                    {
                        return execPath.string();
                    }
                }
            }
        }
#else
        const char* pathEnv = std::getenv("PATH");
        if (pathEnv)
        {
            std::string pathStr(pathEnv);
            std::istringstream iss(pathStr);
            std::string dir;

            while (std::getline(iss, dir, ':'))
            {
                fs::path execPath = fs::path(dir) / name;
                if (fs::exists(execPath))
                {
                    return execPath.string();
                }
            }
        }
#endif

        return "";
    }

    bool ExternalToolDetector::checkQGISProcessing(const std::string& provider)
    {
        if (!qgis_available_)
        {
            return false;
        }

        std::string qgisProcess = findExecutable("qgis_process");
        if (qgisProcess.empty())
        {
            return false;
        }

        ProcessResult result = process_manager_->execute(qgisProcess + " list", std::vector<std::string>{}, "");

        if (result.exit_code == 0)
        {
            return result.output.find(provider) != std::string::npos;
        }

        return false;
    }

}  // namespace soilwatercm::infrastructure
