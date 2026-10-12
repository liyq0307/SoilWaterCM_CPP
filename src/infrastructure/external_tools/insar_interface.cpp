#include "insar_interface.h"

#include <filesystem>
#include <future>
#include <sstream>

#include "utils/logger.h"

#pragma warning(disable : 4996)

namespace soilwatercm::infrastructure
{

    namespace fs = std::filesystem;

    InsarInterface::InsarInterface(const std::string& toolPath, InsarToolType preferredTool)
        : tool_path_(toolPath),
          active_tool_(preferredTool),
          process_manager_(std::make_unique<ProcessManager>()),
          logger_(utils::Logger::getLogger("insar_interface"))
    {
        if (active_tool_ == InsarToolType::NONE)
        {
            active_tool_ = detectAvailableTool();
        }
        else
        {
            if (active_tool_ == InsarToolType::ISCE2 && !checkIsce2Available())
            {
                logger_->warn("ISCE2 not available, trying SNAP");
                if (checkSnapAvailable())
                {
                    active_tool_ = InsarToolType::SNAP;
                }
                else
                {
                    active_tool_ = InsarToolType::NONE;
                }
            }
            else if (active_tool_ == InsarToolType::SNAP && !checkSnapAvailable())
            {
                logger_->warn("SNAP not available, trying ISCE2");
                if (checkIsce2Available())
                {
                    active_tool_ = InsarToolType::ISCE2;
                }
                else
                {
                    active_tool_ = InsarToolType::NONE;
                }
            }
        }

        available_ = (active_tool_ != InsarToolType::NONE);

        if (available_)
        {
            logger_->info("InSAR interface initialized, active tool: {}, version: {}", getName(), version_);
        }
        else
        {
            logger_->warn("No InSAR tool available, interface disabled");
        }
    }

    ToolResult InsarInterface::execute(const std::string& algorithm, const std::map<std::string, std::string>& params,
                                       const ProgressCallback& callback)
    {
        if (!available_)
        {
            return {false, "", "InSAR interface not available", -1, {}};
        }

        logger_->info("Executing InSAR algorithm: {} with tool: {}", algorithm, getName());

        if (active_tool_ == InsarToolType::ISCE2)
        {
            return executeIsce2(algorithm, params, callback);
        }
        else if (active_tool_ == InsarToolType::SNAP)
        {
            return executeSnap(algorithm, params, callback);
        }
        else
        {
            return {false, "", "No InSAR tool configured", -1, {}};
        }
    }

    std::future<ToolResult> InsarInterface::executeAsync(const std::string& algorithm,
                                                         const std::map<std::string, std::string>& params,
                                                         const ProgressCallback& callback)
    {
        return std::async(std::launch::async,
                          [this, algorithm, params, callback]() { return execute(algorithm, params, callback); });
    }

    ToolResult InsarInterface::performDInSAR(const std::string& masterImage, const std::string& slaveImage,
                                             const std::string& demPath, const std::string& outputDir)
    {
        std::map<std::string, std::string> params = {
            {"master", masterImage}, {"slave", slaveImage}, {"dem", demPath}, {"output", outputDir}};

        return execute("dinsar", params);
    }

    ToolResult InsarInterface::performCoregistration(const std::string& masterImage, const std::string& slaveImage,
                                                     const std::string& outputDir)
    {
        std::map<std::string, std::string> params = {
            {"master", masterImage}, {"slave", slaveImage}, {"output", outputDir}};

        return execute("coregistration", params);
    }

    ToolResult InsarInterface::generateInterferogram(const std::string& masterImage, const std::string& slaveImage,
                                                     const std::string& outputDir)
    {
        std::map<std::string, std::string> params = {
            {"master", masterImage}, {"slave", slaveImage}, {"output", outputDir}};

        return execute("interferogram", params);
    }

    ToolResult InsarInterface::unwrapPhase(const std::string& wrappedPhase, const std::string& coherence,
                                           const std::string& output)
    {
        std::map<std::string, std::string> params = {
            {"wrapped", wrappedPhase}, {"coherence", coherence}, {"output", output}};

        return execute("unwrap", params);
    }

    std::vector<AlgorithmInfo> InsarInterface::listAlgorithms()
    {
        return {
            {"dinsar",
             "D-InSAR Processing",
             "Perform two-pass D-InSAR processing",
             {"master", "slave", "dem", "output"}},
            {"coregistration",
             "Image Coregistration",
             "Coregister master and slave images",
             {"master", "slave", "output"}},
            {"interferogram",
             "Interferogram Generation",
             "Generate interferogram from SLC pairs",
             {"master", "slave", "output"}},
            {"unwrap", "Phase Unwrapping", "Unwrap wrapped interferogram phase", {"wrapped", "coherence", "output"}},
            {"filter", "Phase Filtering", "Filter interferogram phase", {"interferogram", "output", "method"}},
            {"geocode", "Geocoding", "Geocode InSAR products to map coordinates", {"input", "dem", "output"}}};
    }

    bool InsarInterface::isAvailable()
    {
        return available_;
    }

    std::string InsarInterface::getVersion()
    {
        return version_;
    }

    std::string InsarInterface::getName()
    {
        switch (active_tool_)
        {
            case InsarToolType::ISCE2:
                return "ISCE2";
            case InsarToolType::SNAP:
                return "SNAP";
            default:
                return "None";
        }
    }

    ToolResult InsarInterface::executeIsce2(const std::string& algorithm,
                                            const std::map<std::string, std::string>& params,
                                            const ProgressCallback& callback)
    {
        std::string command = buildIsce2Command(algorithm, params);

        ProcessResult result = process_manager_->execute(command, std::vector<std::string>{}, "");

        std::string outputFile;
        auto it = params.find("output");
        if (it != params.end())
        {
            outputFile = it->second;
        }

        return {result.exit_code == 0, result.output, result.error, result.exit_code, {{"output", outputFile}}};
    }

    ToolResult InsarInterface::executeSnap(const std::string& algorithm,
                                           const std::map<std::string, std::string>& params,
                                           const ProgressCallback& callback)
    {
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

    InsarToolType InsarInterface::detectAvailableTool()
    {
#ifndef _WIN32
        if (checkIsce2Available())
        {
            return InsarToolType::ISCE2;
        }
#endif

        if (checkSnapAvailable())
        {
            return InsarToolType::SNAP;
        }

        return InsarToolType::NONE;
    }

    std::string InsarInterface::buildIsce2Command(const std::string& algorithm,
                                                  const std::map<std::string, std::string>& params)
    {
        std::ostringstream oss;

        oss << "conda run -n isce2 ";

        if (algorithm == "dinsar")
        {
            oss << "topsApp.py --start=" << params.at("master") << " --end=" << params.at("slave");
        }
        else if (algorithm == "coregistration")
        {
            oss << "isce2/coregistration.py "
                << " --master=" << params.at("master") << " --slave=" << params.at("slave")
                << " --output=" << params.at("output");
        }
        else
        {
            oss << "isce2/" << algorithm << ".py";
            for (const auto& [key, value] : params)
            {
                oss << " --" << key << "=" << value;
            }
        }

        return oss.str();
    }

    std::string InsarInterface::buildSnapCommand(const std::string& algorithm,
                                                 const std::map<std::string, std::string>& params)
    {
        std::ostringstream oss;

        std::string gptPath;
        if (!tool_path_.empty())
        {
            fs::path snapPath(tool_path_);
#ifdef _WIN32
            gptPath = (snapPath / "bin" / "gpt.exe").string();
#else
            gptPath = (snapPath / "bin" / "gpt").string();
#endif
        }
        else
        {
            gptPath = "gpt";
        }

        oss << gptPath;

        if (algorithm == "dinsar")
        {
            oss << " Interferogram "
                << "-PmasterProduct=" << params.at("master") << " -PslaveProduct=" << params.at("slave");
        }
        else if (algorithm == "coregistration")
        {
            oss << " Coregistration "
                << "-PmasterProduct=" << params.at("master") << " -PslaveProduct=" << params.at("slave");
        }
        else
        {
            oss << " " << algorithm;
            for (const auto& [key, value] : params)
            {
                oss << " -P" << key << "=" << value;
            }
        }

        return oss.str();
    }

    bool InsarInterface::checkIsce2Available()
    {
#ifndef _WIN32
        ProcessResult result = process_manager_->execute(
            "conda run -n isce2 python -c \"import isce; print(isce.__version__)\"", std::vector<std::string>{}, "");

        if (result.exit_code == 0)
        {
            version_ = result.output;
            size_t start = version_.find_last_of('\n');
            if (start != std::string::npos)
            {
                version_ = version_.substr(start + 1);
            }
            return true;
        }
#endif
        return false;
    }

    bool InsarInterface::checkSnapAvailable()
    {
        std::string gptPath;
        if (!tool_path_.empty())
        {
            fs::path snapPath(tool_path_);
#ifdef _WIN32
            gptPath = (snapPath / "bin" / "gpt.exe").string();
#else
            gptPath = (snapPath / "bin" / "gpt").string();
#endif
        }
        else
        {
#ifdef _WIN32
            gptPath = "gpt.exe";
#else
            gptPath = "gpt";
#endif
        }

        ProcessResult result = process_manager_->execute(gptPath + " -v", std::vector<std::string>{}, "");

        if (result.exit_code == 0)
        {
            std::string output = result.output;
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

}  // namespace soilwatercm::infrastructure
