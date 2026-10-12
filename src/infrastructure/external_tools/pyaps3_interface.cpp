#include "pyaps3_interface.h"

#include <filesystem>
#include <future>
#include <sstream>

#include "utils/logger.h"

namespace soilwatercm::infrastructure
{

    namespace fs = std::filesystem;

    Pyaps3Interface::Pyaps3Interface(const std::string& condaEnv)
        : conda_env_(condaEnv),
          process_manager_(std::make_unique<ProcessManager>()),
          logger_(utils::Logger::getLogger("pyaps3_interface"))
    {
        available_ = checkPyaps3Available();

        if (available_)
        {
            logger_->info("pyaps3 interface initialized, conda env: {}, version: {}", conda_env_, version_);
        }
        else
        {
            logger_->warn("pyaps3 not available in conda env: {}, interface disabled", conda_env_);
        }
    }

    ToolResult Pyaps3Interface::execute(const std::string& algorithm, const std::map<std::string, std::string>& params,
                                        const ProgressCallback& callback)
    {
        if (!available_)
        {
            return {false, "", "pyaps3 interface not available", -1, {}};
        }

        logger_->info("Executing pyaps3 algorithm: {}", algorithm);

        std::string command = buildPyaps3Command(algorithm, params);

        ProcessResult result = process_manager_->execute(command, std::vector<std::string>{}, "");

        std::string outputFile;
        auto it = params.find("output");
        if (it != params.end())
        {
            outputFile = it->second;
        }

        return {result.exit_code == 0, result.output, result.error, result.exit_code, {{"output", outputFile}}};
    }

    std::future<ToolResult> Pyaps3Interface::executeAsync(const std::string& algorithm,
                                                          const std::map<std::string, std::string>& params,
                                                          const ProgressCallback& callback)
    {
        return std::async(std::launch::async,
                          [this, algorithm, params, callback]() { return execute(algorithm, params, callback); });
    }

    ToolResult Pyaps3Interface::downloadAtmosphericData(const std::string& dateStart, const std::string& dateEnd,
                                                        const std::string& model, const std::string& outputDir)
    {
        std::map<std::string, std::string> params = {
            {"date_start", dateStart}, {"date_end", dateEnd}, {"model", model}, {"output", outputDir}};

        std::ostringstream oss;
        oss << "conda run -n " << conda_env_ << " python -c \"from pyaps3 import get_weather_model; "
            << "get_weather_model('" << model << "', '" << dateStart << "', '" << dateEnd << "', '" << outputDir
            << "')\"";

        ProcessResult result = process_manager_->execute(oss.str(), std::vector<std::string>{}, "");

        return {result.exit_code == 0, result.output, result.error, result.exit_code, {{"output", outputDir}}};
    }

    ToolResult Pyaps3Interface::calculateTroposphericDelay(const std::string& atmosDataDir,
                                                           const std::string& geometryFile, const std::string& output)
    {
        std::map<std::string, std::string> params = {
            {"atmos_dir", atmosDataDir}, {"geometry", geometryFile}, {"output", output}};

        std::ostringstream oss;
        oss << "conda run -n " << conda_env_ << " python -c \"from pyaps3 import calculate_delay; "
            << "calculate_delay('" << atmosDataDir << "', '" << geometryFile << "', '" << output << "')\"";

        ProcessResult result = process_manager_->execute(oss.str(), std::vector<std::string>{}, "");

        return {result.exit_code == 0, result.output, result.error, result.exit_code, {{"output", output}}};
    }

    ToolResult Pyaps3Interface::correctInterferogram(const std::string& ifgramFile, const std::string& tropoFile,
                                                     const std::string& output)
    {
        std::map<std::string, std::string> params = {{"ifgram", ifgramFile}, {"tropo", tropoFile}, {"output", output}};

        std::ostringstream oss;
        oss << "conda run -n " << conda_env_ << " python -c \"from pyaps3 import correct_ifgram; "
            << "correct_ifgram('" << ifgramFile << "', '" << tropoFile << "', '" << output << "')\"";

        ProcessResult result = process_manager_->execute(oss.str(), std::vector<std::string>{}, "");

        return {result.exit_code == 0, result.output, result.error, result.exit_code, {{"output", output}}};
    }

    bool Pyaps3Interface::correctAtmosphere(const std::string& inputPath, const std::string& outputPath)
    {
        std::ostringstream oss;
        oss << "conda run -n " << conda_env_ << " python -c \""
            << "import pyaps3 as pa; "
            << "dc = pa.DECORR('" << inputPath << "', ERA5); "
            << "dc.estimate_delay(); "
            << "dc.apply_delay('" << outputPath << "')\"";

        ProcessResult result = process_manager_->execute(oss.str(), std::vector<std::string>{}, "");

        return result.exit_code == 0;
    }

    std::vector<AlgorithmInfo> Pyaps3Interface::listAlgorithms()
    {
        return {{"get_weather_model",
                 "Download Atmospheric Model Data (ERA5/MERRA/NCEP)",
                 "Download ERA5/MERRA/NCEP weather model data",
                 {"model", "date_start", "date_end", "output"}},
                {"calculate_delay",
                 "Calculate Tropospheric Delay",
                 "Calculate tropospheric delay from weather data",
                 {"atmos_dir", "geometry", "output"}},
                {"correct_ifgram",
                 "Correct Interferogram with Tropospheric Delay",
                 "Correct interferogram using tropospheric delay data",
                 {"ifgram", "tropo", "output"}}};
    }

    bool Pyaps3Interface::isAvailable()
    {
        return available_;
    }

    std::string Pyaps3Interface::getVersion()
    {
        return version_;
    }

    std::string Pyaps3Interface::buildCondaCommand(const std::string& script)
    {
        std::ostringstream oss;
        oss << "conda run -n " << conda_env_ << " " << script;
        return oss.str();
    }

    std::string Pyaps3Interface::buildPyaps3Command(const std::string& algorithm,
                                                    const std::map<std::string, std::string>& params)
    {
        std::ostringstream oss;

        oss << "conda run -n " << conda_env_ << " python -c \"from pyaps3 import " << algorithm << "; " << algorithm
            << "()\"";

        for (const auto& [key, value] : params)
        {
            oss << " --" << key << " " << value;
        }

        return oss.str();
    }

    bool Pyaps3Interface::checkPyaps3Available()
    {
        std::ostringstream oss;
        oss << "conda run -n " << conda_env_ << " python -c \"import pyaps3; print(pyaps3.__version__)\"";

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

        oss.str("");
        oss << "conda run -n " << conda_env_ << " python -c \"import pyaps3; print('available')\"";

        result = process_manager_->execute(oss.str(), std::vector<std::string>{}, "");

        if (result.exit_code == 0)
        {
            version_ = "unknown";
            return true;
        }

        return false;
    }

}  // namespace soilwatercm::infrastructure
