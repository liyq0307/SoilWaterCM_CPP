#include "mintpy_interface.h"

#include <filesystem>
#include <fstream>
#include <future>
#include <sstream>

#include "utils/logger.h"

namespace soilwatercm::infrastructure
{

    namespace fs = std::filesystem;

    MintPyInterface::MintPyInterface(const std::string& condaEnv)
        : conda_env_(condaEnv),
          process_manager_(std::make_unique<ProcessManager>()),
          logger_(utils::Logger::getLogger("mintpy_interface"))
    {
        available_ = checkMintPyAvailable();

        if (available_)
        {
            logger_->info("MintPy interface initialized, conda env: {}, version: {}", conda_env_, version_);
        }
        else
        {
            logger_->warn("MintPy not available in conda env: {}, interface disabled", conda_env_);
        }
    }

    ToolResult MintPyInterface::execute(const std::string& algorithm, const std::map<std::string, std::string>& params,
                                        const ProgressCallback& callback)
    {
        if (!available_)
        {
            return {false, "", "MintPy interface not available", -1, {}};
        }

        logger_->info("Executing MintPy algorithm: {}", algorithm);

        std::string command = buildMintPyCommand(algorithm, params);

        ProcessResult result = process_manager_->execute(command, std::vector<std::string>{}, "");

        std::string outputFile;
        auto it = params.find("output");
        if (it != params.end())
        {
            outputFile = it->second;
        }

        return {result.exit_code == 0, result.output, result.error, result.exit_code, {{"output", outputFile}}};
    }

    std::future<ToolResult> MintPyInterface::executeAsync(const std::string& algorithm,
                                                          const std::map<std::string, std::string>& params,
                                                          const ProgressCallback& callback)
    {
        return std::async(std::launch::async,
                          [this, algorithm, params, callback]() { return execute(algorithm, params, callback); });
    }

    ToolResult MintPyInterface::runSmallBaselineApp(const std::string& templateFile, const std::string& projectDir)
    {
        std::map<std::string, std::string> params = {{"template", templateFile}, {"workdir", projectDir}};

        std::string command =
            buildCondaCommand("smallbaselineApp.py") + " " + templateFile + " --workdir " + projectDir;

        ProcessResult result = process_manager_->execute(command, std::vector<std::string>{}, "");

        return {result.exit_code == 0, result.output, result.error, result.exit_code, {{"output", projectDir}}};
    }

    ToolResult MintPyInterface::generateVelocityMap(const std::string& projectDir, const std::string& output)
    {
        std::map<std::string, std::string> params = {{"workdir", projectDir}, {"output", output}};

        std::string command = buildCondaCommand("velocity.py") + " " +
                              (fs::path(projectDir) / "inputs" / "ifgramStack.h5").string() + " -o " + output;

        ProcessResult result = process_manager_->execute(command, std::vector<std::string>{}, "");

        return {result.exit_code == 0, result.output, result.error, result.exit_code, {{"output", output}}};
    }

    ToolResult MintPyInterface::generateTimeSeries(const std::string& projectDir, const std::string& output)
    {
        std::map<std::string, std::string> params = {{"workdir", projectDir}, {"output", output}};

        std::string command = buildCondaCommand("timeseries2velocity.py") + " " +
                              (fs::path(projectDir) / "timeseries.h5").string() + " -o " + output;

        ProcessResult result = process_manager_->execute(command, std::vector<std::string>{}, "");

        return {result.exit_code == 0, result.output, result.error, result.exit_code, {{"output", output}}};
    }

    ToolResult MintPyInterface::correctAtmosphere(const std::string& projectDir, const std::string& method)
    {
        std::map<std::string, std::string> params = {{"workdir", projectDir}, {"method", method}};

        std::string command = buildCondaCommand("correct_atmosphere.py") + " " +
                              (fs::path(projectDir) / "timeseries.h5").string() + " --method " + method;

        ProcessResult result = process_manager_->execute(command, std::vector<std::string>{}, "");

        return {result.exit_code == 0, result.output, result.error, result.exit_code, {{"output", projectDir}}};
    }

    ToolResult MintPyInterface::estimateDemError(const std::string& projectDir, const std::string& output)
    {
        std::map<std::string, std::string> params = {{"workdir", projectDir}, {"output", output}};

        std::string command = buildCondaCommand("dem_error.py") + " " +
                              (fs::path(projectDir) / "timeseries.h5").string() + " -o " + output;

        ProcessResult result = process_manager_->execute(command, std::vector<std::string>{}, "");

        return {result.exit_code == 0, result.output, result.error, result.exit_code, {{"output", output}}};
    }

    std::vector<AlgorithmInfo> MintPyInterface::listAlgorithms()
    {
        return {{"smallbaselineApp", "Small Baseline InSAR Time Series Analysis", "", {"template", "workdir"}},
                {"velocity", "Generate Velocity Map", "", {"ifgramStack", "output"}},
                {"timeseries2velocity", "Convert Time Series to Velocity", "", {"timeseries", "output"}},
                {"correct_atmosphere", "Atmospheric Correction (ERA5/MERRA)", "", {"timeseries", "method"}},
                {"dem_error", "DEM Error Correction", "", {"timeseries", "output"}},
                {"ifgram_inversion", "Interferogram Inversion", "", {"ifgramStack", "output"}},
                {"network_inversion", "Network Inversion", "", {"ifgramStack", "output"}},
                {"reference_point", "Set Reference Point", "", {"timeseries", "coord"}},
                {"mask", "Generate Mask", "", {"timeseries", "output"}}};
    }

    bool MintPyInterface::isAvailable()
    {
        return available_;
    }

    std::string MintPyInterface::getVersion()
    {
        return version_;
    }

    std::string MintPyInterface::buildCondaCommand(const std::string& script)
    {
        std::ostringstream oss;
        oss << "conda run -n " << conda_env_ << " " << script;
        return oss.str();
    }

    std::string MintPyInterface::buildMintPyCommand(const std::string& algorithm,
                                                    const std::map<std::string, std::string>& params)
    {
        std::ostringstream oss;

        oss << "conda run -n " << conda_env_ << " " << algorithm << ".py";

        for (const auto& [key, value] : params)
        {
            oss << " --" << key << " " << value;
        }

        return oss.str();
    }

    bool MintPyInterface::checkMintPyAvailable()
    {
        std::ostringstream oss;
        oss << "conda run -n " << conda_env_ << " python -c \"import mintpy; print(mintpy.__version__)\"";

        ProcessResult result = process_manager_->execute(oss.str(), std::vector<std::string>{}, "");

        if (result.exit_code == 0)
        {
            version_ = result.output;
            size_t start = version_.find_last_of('\n');
            if (start != std::string::npos)
            {
                version_ = version_.substr(start + 1);
            }
            size_t end = version_.find('\n');
            if (end != std::string::npos)
            {
                version_ = version_.substr(0, end);
            }
            return true;
        }

        return false;
    }

    bool MintPyInterface::generateConfig(const std::string& projectDir, const std::string& configPath)
    {
        std::map<std::string, std::string> params;
        return writeSmallBaselineTemplate(configPath, params);
    }

    bool MintPyInterface::runSbas(const std::string& configPath, const std::string& outputDir)
    {
        std::string command = buildCondaCommand("smallbaselineApp.py") + " " + configPath + " --workdir " + outputDir;

        ProcessResult result = process_manager_->execute(command, std::vector<std::string>{}, "");

        return result.exit_code == 0;
    }

    bool MintPyInterface::writeSmallBaselineTemplate(const std::string& templatePath,
                                                     const std::map<std::string, std::string>& params)
    {
        try
        {
            fs::path p(templatePath);
            fs::path parentDir = p.parent_path();
            if (!fs::exists(parentDir))
            {
                fs::create_directories(parentDir);
            }

            std::ofstream ofs(templatePath);
            if (!ofs.is_open())
            {
                logger_->error("Failed to open template file: {}", templatePath);
                return false;
            }

            ofs << "## MintPy Small Baseline Configuration\n";
            ofs << "mintpy.load.processor        = isce\n";
            ofs << "mintpy.load.unwFile          = ./merged/interferograms/*/filt_*.unw\n";
            ofs << "mintpy.load.corFile          = ./merged/interferograms/*/filt_*.cor\n";
            ofs << "mintpy.load.demFile          = ./merged/geom_reference/hgt.rdr\n";
            ofs << "mintpy.load.metaFile         = ./merged/reference/*.xml\n";
            ofs << "mintpy.load.bperpFile        = ./merged/baseline/*.bperp\n";
            ofs << "\n";
            ofs << "mintpy.network.connNumMax    = 200\n";
            ofs << "mintpy.network.connCompMin   = 0.5\n";
            ofs << "\n";
            ofs << "mintpy.reference.lalo       = auto\n";
            ofs << "\n";
            ofs << "mintpy.troposphericDelay.method = pyaps\n";
            ofs << "mintpy.troposphericDelay.weatherModel = ERA5\n";
            ofs << "\n";
            ofs << "mintpy.deramp               = no\n";
            ofs << "mintpy.topoResidual         = no\n";
            ofs << "\n";
            ofs << "mintpy.temporalCoherence     = 0.7\n";
            ofs << "mintpy.samplingMethod       = nearest\n";

            for (const auto& [key, value] : params)
            {
                ofs << key << " = " << value << "\n";
            }

            ofs.close();
            return true;
        }
        catch (const std::exception& e)
        {
            logger_->error("Failed to write template file: {}", e.what());
            return false;
        }
    }

}  // namespace soilwatercm::infrastructure
