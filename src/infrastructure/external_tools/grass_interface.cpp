#include "grass_interface.h"

#include <regex>
#include <sstream>

#include "../../utils/logger.h"
#include "process_manager.h"

namespace soilwatercm::infrastructure::external_tools
{

    GrassInterface::GrassInterface() : grass_path_("grass")
    {
        processMgr_ = std::make_unique<ProcessManager>();
    }

    GrassInterface::GrassInterface(const std::string& grass_path) : grass_path_(grass_path)
    {
        processMgr_ = std::make_unique<ProcessManager>();
    }
    bool GrassInterface::isAvailable()
    {
        ProcessResult result = processMgr_->execute(grass_path_, {"--version"});
        return result.exit_code == 0;
    }

    ToolInfo GrassInterface::getInfo() const
    {
        ToolInfo info;
        info.name = "GRASS GIS";
        info.path = grass_path_;
        ProcessResult result = processMgr_->execute(grass_path_, {"--version"});
        info.available = (result.exit_code == 0);

        if (info.available)
        {
            std::smatch match;
            if (std::regex_search(result.output, match, std::regex(R"((\d+\.\d+\.\d+))")))
            {
                info.version = match[1];
            }
            else
            {
                info.version = "unknown";
            }
        }
        return info;
    }

    bool GrassInterface::checkVersion(const std::string& min_version) const
    {
        ProcessResult result = processMgr_->execute(grass_path_, {"--version"});
        return result.exit_code == 0;
    }

    bool GrassInterface::execute(const std::vector<std::string>& args, const std::string& working_dir)
    {
        ProcessResult result = processMgr_->executeWithTimeout(grass_path_, args, timeout_seconds_, working_dir);
        last_output_ = result.output;
        last_error_ = result.error;
        last_exit_code_ = result.exit_code;
        return result.exit_code == 0 && !result.timed_out;
    }

    bool GrassInterface::executeAsync(const std::vector<std::string>& args, const std::string& working_dir)
    {
        processMgr_->executeAsync(grass_path_, args, working_dir);
        return true;
    }

    std::string GrassInterface::getOutput() const
    {
        return last_output_;
    }

    std::string GrassInterface::getError() const
    {
        return last_error_;
    }

    int GrassInterface::getExitCode() const
    {
        return last_exit_code_;
    }

    void GrassInterface::setTimeout(int seconds)
    {
        timeout_seconds_ = seconds;
    }

    std::string GrassInterface::getVersion()
    {
        ProcessResult result = processMgr_->execute(grass_path_, {"--version"});
        std::smatch match;
        if (std::regex_search(result.output, match, std::regex(R"((\d+\.\d+\.\d+))")))
        {
            return match[1];
        }
        return "unknown";
    }

    ToolResult GrassInterface::execute(const std::string& algorithm, const std::map<std::string, std::string>& params,
                                       const ProgressCallback& callback)
    {
        std::vector<std::string> args = {algorithm};
        for (const auto& [key, value] : params)
        {
            args.push_back(key + "=" + value);
        }
        ProcessResult result = processMgr_->execute(grass_path_, args);
        if (callback)
        {
            callback(1.0, "GRASS execution complete");
        }
        return {result.exit_code == 0, result.output, result.error, result.exit_code, {}};
    }

    std::future<ToolResult> GrassInterface::executeAsync(const std::string& algorithm,
                                                         const std::map<std::string, std::string>& params,
                                                         const ProgressCallback& callback)
    {
        return std::async(std::launch::async,
                          [this, algorithm, params, callback]() { return execute(algorithm, params, callback); });
    }

    std::vector<AlgorithmInfo> GrassInterface::listAlgorithms()
    {
        return {};
    }

    void GrassInterface::setEnvironment(const std::map<std::string, std::string>& env)
    {
        processMgr_->setEnvironment(env);
    }

    bool GrassInterface::runSlopeAspect(const std::string& dem_path, const std::string& output_slope,
                                        const std::string& output_aspect)
    {
        utils::Logger::info("GRASS: computing slope/aspect from {}", dem_path);
        return execute({"r.slope.aspect", "elevation=" + dem_path, "slope=" + output_slope, "aspect=" + output_aspect,
                        "--overwrite"});
    }

    bool GrassInterface::runWatershed(const std::string& dem_path, const std::string& output_dir)
    {
        utils::Logger::info("GRASS: computing watershed from {}", dem_path);
        return execute({"r.watershed", "elevation=" + dem_path, "accumulation=" + output_dir + "/accumulation.tif",
                        "drainage=" + output_dir + "/drainage.tif", "basin=" + output_dir + "/basin.tif",
                        "stream=" + output_dir + "/stream.tif", "--overwrite"});
    }

    bool GrassInterface::runMorphology(const std::string& input_path, const std::string& output_path,
                                       const std::string& operation)
    {
        utils::Logger::info("GRASS: running morphology {} on {}", operation, input_path);
        std::string op = (operation == "opening") ? "open" : (operation == "closing") ? "close" : operation;
        return execute(
            {"r.morphology", "input=" + input_path, "output=" + output_path, "operation=" + op, "--overwrite"});
    }

    bool GrassInterface::calculateLSFactor(const std::string& demPath, const std::string& outputPath)
    {
        utils::Logger::info("GRASS: calculating LS factor from {}", demPath);

        std::string slopePath = outputPath + "_slope.tif";
        std::string aspectPath = outputPath + "_aspect.tif";

        if (!runSlopeAspect(demPath, slopePath, aspectPath))
        {
            utils::Logger::warn("GRASS: slope/aspect calculation failed, using built-in method");
            return false;
        }

        return execute({"r.mapcalc",
                        "expression=\"" + outputPath +
                            " = (\" + slopePath + \" / 22.13) ^ 0.5 * (sin(radians(\" + slopePath + \")) * 65.41 + "
                            "4.56 * sin(radians(\" + slopePath + \")) + 0.065)\"",
                        "--overwrite"});
    }

    bool GrassInterface::morphologyProcess(const std::string& inputPath, const std::string& outputPath,
                                           const std::string& operation)
    {
        utils::Logger::info("GRASS: morphology process '{}' on {}", operation, inputPath);

        if (operation == "opening" || operation == "closing")
        {
            return runMorphology(inputPath, outputPath, operation);
        }
        else if (operation == "erosion")
        {
            return execute(
                {"r.morphology", "input=" + inputPath, "output=" + outputPath, "operation=erosion", "--overwrite"});
        }
        else if (operation == "dilation")
        {
            return execute(
                {"r.morphology", "input=" + inputPath, "output=" + outputPath, "operation=dilation", "--overwrite"});
        }

        return runMorphology(inputPath, outputPath, operation);
    }

}  // namespace soilwatercm::infrastructure::external_tools
