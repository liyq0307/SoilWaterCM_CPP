#include "snap_interface.h"

#include <filesystem>
#include <fstream>
#include <future>
#include <sstream>

#include "utils/logger.h"

#pragma warning(disable : 4996)

namespace soilwatercm::infrastructure
{

    namespace fs = std::filesystem;

    SnapInterface::SnapInterface(const std::string& snapPath)
        : snap_path_(snapPath),
          process_manager_(std::make_unique<ProcessManager>()),
          logger_(utils::Logger::getLogger("snap_interface"))
    {
        gpt_path_ = findGptPath();
        available_ = detectSnapInstallation();

        if (available_)
        {
            logger_->info("SNAP interface initialized, path: {}, version: {}", snap_path_, version_);
        }
        else
        {
            logger_->warn("SNAP not found, interface disabled");
        }
    }

    ToolResult SnapInterface::execute(const std::string& algorithm, const std::map<std::string, std::string>& params,
                                      const ProgressCallback& callback)
    {
        if (!available_)
        {
            return {false, "", "SNAP interface not available", -1, {}};
        }

        logger_->info("Executing SNAP algorithm: {}", algorithm);

        std::string command = buildSnapCommand(algorithm, params);

        ProcessResult result = process_manager_->execute(command, std::vector<std::string>{}, "");

        std::string outputFile;
        auto it = params.find("output");
        if (it != params.end())
        {
            outputFile = it->second;
        }

        return {result.exit_code == 0, result.output, result.error, result.exit_code, {{"output", outputFile}}};
    }

    std::future<ToolResult> SnapInterface::executeAsync(const std::string& algorithm,
                                                        const std::map<std::string, std::string>& params,
                                                        const ProgressCallback& callback)
    {
        return std::async(std::launch::async,
                          [this, algorithm, params, callback]() { return execute(algorithm, params, callback); });
    }

    ToolResult SnapInterface::performInSAR(const std::string& masterProduct, const std::string& slaveProduct,
                                           const std::string& outputDir)
    {
        std::map<std::string, std::string> params = {
            {"master", masterProduct}, {"slave", slaveProduct}, {"output", outputDir}};

        return execute("Interferogram", params);
    }

    ToolResult SnapInterface::coregisterImages(const std::string& masterProduct, const std::string& slaveProduct,
                                               const std::string& outputDir)
    {
        std::map<std::string, std::string> params = {
            {"master", masterProduct}, {"slave", slaveProduct}, {"output", outputDir}};

        return execute("Back-Geocoding", params);
    }

    ToolResult SnapInterface::generateInterferogram(const std::string& coregisteredProduct,
                                                    const std::string& outputDir)
    {
        std::map<std::string, std::string> params = {{"input", coregisteredProduct}, {"output", outputDir}};

        return execute("Interferogram", params);
    }

    ToolResult SnapInterface::deburstAndMerge(const std::string& productPath, const std::string& output)
    {
        std::map<std::string, std::string> params = {{"input", productPath}, {"output", output}};

        return execute("TOPSAR-Deburst", params);
    }

    ToolResult SnapInterface::applyOrbitFile(const std::string& productPath, const std::string& output)
    {
        std::map<std::string, std::string> params = {{"input", productPath}, {"output", output}};

        return execute("Apply-Orbit-File", params);
    }

    ToolResult SnapInterface::terrainFlattening(const std::string& productPath, const std::string& demPath,
                                                const std::string& output)
    {
        std::map<std::string, std::string> params = {{"input", productPath}, {"dem", demPath}, {"output", output}};

        return execute("Terrain-Flattening", params);
    }

    std::vector<AlgorithmInfo> SnapInterface::listAlgorithms()
    {
        return {
            {"Interferogram",
             "Interferogram Generation",
             "Generate interferogram from SLC products",
             {"master", "slave", "output"}},
            {"Back-Geocoding",
             "Back Geocoding Coregistration",
             "Coregister master and slave images",
             {"master", "slave", "output"}},
            {"TOPSAR-Deburst", "TOPSAR Deburst and Merge", "Deburst and merge TOPSAR SLC data", {"input", "output"}},
            {"Apply-Orbit-File", "Apply Orbit File", "Apply precise orbit file to product", {"input", "output"}},
            {"Terrain-Flattening", "Terrain Flattening", "Flatten interferogram with DEM", {"input", "dem", "output"}},
            {"Terrain-Correction",
             "Range Doppler Terrain Correction",
             "Apply terrain correction to product",
             {"input", "dem", "output"}},
            {"Goldstein-Phase-Filter",
             "Goldstein Phase Filter",
             "Filter phase using Goldstein method",
             {"input", "output"}},
            {"BandMath", "Band Math", "Execute band math expression", {"input", "expression", "output"}},
            {"Subset", "Spatial Subset", "Create spatial subset of product", {"input", "geometry", "output"}}};
    }

    bool SnapInterface::isAvailable()
    {
        return available_;
    }

    std::string SnapInterface::getVersion()
    {
        return version_;
    }

    std::string SnapInterface::buildGptCommand(const std::string& graphPath,
                                               const std::map<std::string, std::string>& params)
    {
        std::ostringstream oss;
        oss << gpt_path_ << " " << graphPath;

        for (const auto& [key, value] : params)
        {
            oss << " -P" << key << "=" << value;
        }

        return oss.str();
    }

    std::string SnapInterface::buildSnapCommand(const std::string& algorithm,
                                                const std::map<std::string, std::string>& params)
    {
        std::ostringstream oss;
        oss << gpt_path_ << " " << algorithm;

        for (const auto& [key, value] : params)
        {
            oss << " -P" << key << "=" << value;
        }

        return oss.str();
    }

    bool SnapInterface::detectSnapInstallation()
    {
        if (gpt_path_.empty())
        {
            return false;
        }

        ProcessResult result = process_manager_->execute(gpt_path_ + " -v", std::vector<std::string>{}, "");

        if (result.exit_code == 0)
        {
            std::string output = result.output + result.error;
            size_t pos = output.find("SNAP");
            if (pos != std::string::npos)
            {
                size_t start = pos;
                size_t end = output.find('\n', start);
                if (end != std::string::npos)
                {
                    version_ = output.substr(start, end - start);
                }
                else
                {
                    version_ = output.substr(start);
                }
            }
            return true;
        }

        return false;
    }

    std::string SnapInterface::findGptPath()
    {
        if (!snap_path_.empty())
        {
            fs::path snapPath(snap_path_);
#ifdef _WIN32
            fs::path gptPath = snapPath / "bin" / "gpt.exe";
#else
            fs::path gptPath = snapPath / "bin" / "gpt";
#endif
            if (fs::exists(gptPath))
            {
                return gptPath.string();
            }
        }

#ifdef _WIN32
        const char* programFiles = std::getenv("ProgramFiles");
        if (programFiles)
        {
            fs::path defaultPath = fs::path(programFiles) / "snap" / "bin" / "gpt.exe";
            if (fs::exists(defaultPath))
            {
                return defaultPath.string();
            }
        }

        const char* programFilesX86 = std::getenv("ProgramFiles(x86)");
        if (programFilesX86)
        {
            fs::path defaultPath = fs::path(programFilesX86) / "snap" / "bin" / "gpt.exe";
            if (fs::exists(defaultPath))
            {
                return defaultPath.string();
            }
        }
#else
        std::vector<std::string> searchPaths = {"/usr/local/snap/bin/gpt", "/opt/snap/bin/gpt", "/usr/bin/gpt",
                                                "/usr/local/bin/gpt"};

        for (const auto& path : searchPaths)
        {
            if (fs::exists(path))
            {
                return path;
            }
        }
#endif

        return "";
    }

    bool SnapInterface::writeGraphXml(const std::string& graphPath, const std::string& algorithm,
                                      const std::map<std::string, std::string>& params)
    {
        try
        {
            fs::path p(graphPath);
            fs::path parentDir = p.parent_path();
            if (!fs::exists(parentDir))
            {
                fs::create_directories(parentDir);
            }

            std::ofstream ofs(graphPath);
            if (!ofs.is_open())
            {
                logger_->error("Failed to open graph file: {}", graphPath);
                return false;
            }

            ofs << "<graph id=\"1\">\n";
            ofs << "  <version>1.0</version>\n";
            ofs << "  <node id=\"1\">\n";
            ofs << "    <operator>" << algorithm << "</operator>\n";

            for (const auto& [key, value] : params)
            {
                ofs << "    <parameter name=\"" << key << "\">" << value << "</parameter>\n";
            }

            ofs << "  </node>\n";
            ofs << "</graph>\n";
            ofs.close();

            return true;
        }
        catch (const std::exception& e)
        {
            logger_->error("Failed to write graph XML: {}", e.what());
            return false;
        }
    }

}  // namespace soilwatercm::infrastructure
