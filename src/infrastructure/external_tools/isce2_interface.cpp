#include "isce2_interface.h"

#include <filesystem>
#include <fstream>
#include <future>
#include <sstream>

#include "utils/logger.h"

namespace soilwatercm::infrastructure
{

    namespace fs = std::filesystem;

    Isce2Interface::Isce2Interface(const std::string& condaEnv)
        : conda_env_(condaEnv),
          process_manager_(std::make_unique<ProcessManager>()),
          logger_(utils::Logger::getLogger("isce2_interface"))
    {
        available_ = checkIsce2Available();

        if (available_)
        {
            logger_->info("ISCE2 interface initialized, conda env: {}, version: {}", conda_env_, version_);
        }
        else
        {
            logger_->warn("ISCE2 not available in conda env: {}, interface disabled", conda_env_);
        }
    }

    ToolResult Isce2Interface::execute(const std::string& algorithm, const std::map<std::string, std::string>& params,
                                       const ProgressCallback& callback)
    {
        if (!available_)
        {
            return {false, "", "ISCE2 interface not available", -1, {}};
        }

        logger_->info("Executing ISCE2 algorithm: {}", algorithm);

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

    std::future<ToolResult> Isce2Interface::executeAsync(const std::string& algorithm,
                                                         const std::map<std::string, std::string>& params,
                                                         const ProgressCallback& callback)
    {
        return std::async(std::launch::async,
                          [this, algorithm, params, callback]() { return execute(algorithm, params, callback); });
    }

    ToolResult Isce2Interface::runTopsApp(const std::string& masterOrbit, const std::string& slaveOrbit,
                                          const std::string& demPath, const std::string& outputDir,
                                          const std::map<std::string, std::string>& extraParams)
    {
        std::map<std::string, std::string> params = {
            {"master", masterOrbit}, {"slave", slaveOrbit}, {"dem", demPath}, {"output", outputDir}};

        for (const auto& [key, value] : extraParams)
        {
            params[key] = value;
        }

        fs::path configPath = fs::path(outputDir) / "topsApp.cfg";
        if (!writeTopsAppConfig(configPath.string(), params))
        {
            return {false, "", "Failed to write topsApp configuration", -1, {}};
        }

        std::string command = buildCondaCommand("topsApp.py") + " " + configPath.string();

        ProcessResult result = process_manager_->execute(command, std::vector<std::string>{}, "");

        return {result.exit_code == 0, result.output, result.error, result.exit_code, {{"output", outputDir}}};
    }

    ToolResult Isce2Interface::runStripmapApp(const std::string& masterImage, const std::string& slaveImage,
                                              const std::string& demPath, const std::string& outputDir)
    {
        std::map<std::string, std::string> params = {
            {"master", masterImage}, {"slave", slaveImage}, {"dem", demPath}, {"output", outputDir}};

        return execute("stripmapApp", params);
    }

    ToolResult Isce2Interface::coregisterImages(const std::string& masterImage, const std::string& slaveImage,
                                                const std::string& outputDir)
    {
        std::map<std::string, std::string> params = {
            {"master", masterImage}, {"slave", slaveImage}, {"output", outputDir}};

        return execute("coregister", params);
    }

    ToolResult Isce2Interface::generateInterferogram(const std::string& masterImage, const std::string& slaveImage,
                                                     const std::string& outputDir)
    {
        std::map<std::string, std::string> params = {
            {"master", masterImage}, {"slave", slaveImage}, {"output", outputDir}};

        return execute("interferogram", params);
    }

    std::vector<AlgorithmInfo> Isce2Interface::listAlgorithms()
    {
        return {{"topsApp", "TOPS InSAR Processing (Sentinel-1)", "", {"master", "slave", "dem", "output"}},
                {"stripmapApp", "Stripmap InSAR Processing", "", {"master", "slave", "dem", "output"}},
                {"coregister", "Image Coregistration", "", {"master", "slave", "output"}},
                {"interferogram", "Interferogram Generation", "", {"master", "slave", "output"}},
                {"unwrap", "Phase Unwrapping (SNAPHU)", "", {"wrapped", "coherence", "output"}},
                {"geocode", "Geocoding", "", {"input", "dem", "output"}},
                {"denseoffsets", "Dense Offset Tracking", "", {"master", "slave", "output"}}};
    }

    bool Isce2Interface::isAvailable()
    {
        return available_;
    }

    std::string Isce2Interface::getVersion()
    {
        return version_;
    }

    std::string Isce2Interface::buildCondaCommand(const std::string& script)
    {
        std::ostringstream oss;
        oss << "conda run -n " << conda_env_ << " python " << script;
        return oss.str();
    }

    std::string Isce2Interface::buildIsce2Command(const std::string& algorithm,
                                                  const std::map<std::string, std::string>& params)
    {
        std::ostringstream oss;

        oss << "conda run -n " << conda_env_ << " ";

        if (algorithm == "topsApp")
        {
            oss << "topsApp.py";
        }
        else if (algorithm == "stripmapApp")
        {
            oss << "stripmapApp.py";
        }
        else
        {
            oss << "python -c \"from isce.app import " << algorithm << "; " << algorithm << "().run()\"";
        }

        for (const auto& [key, value] : params)
        {
            oss << " --" << key << " " << value;
        }

        return oss.str();
    }

    bool Isce2Interface::checkIsce2Available()
    {
        std::ostringstream oss;
        oss << "conda run -n " << conda_env_ << " python -c \"import isce; print(isce.__version__)\"";

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

    bool Isce2Interface::writeTopsAppConfig(const std::string& configPath,
                                            const std::map<std::string, std::string>& params)
    {
        try
        {
            fs::path p(configPath);
            fs::path parentDir = p.parent_path();
            if (!fs::exists(parentDir))
            {
                fs::create_directories(parentDir);
            }

            std::ofstream ofs(configPath);
            if (!ofs.is_open())
            {
                logger_->error("Failed to open config file: {}", configPath);
                return false;
            }

            ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
            ofs << "<config>\n";

            auto masterIt = params.find("master");
            if (masterIt != params.end())
            {
                ofs << "  <master orbit=\"" << masterIt->second << "\"/>\n";
            }

            auto slaveIt = params.find("slave");
            if (slaveIt != params.end())
            {
                ofs << "  <slave orbit=\"" << slaveIt->second << "\"/>\n";
            }

            auto demIt = params.find("dem");
            if (demIt != params.end())
            {
                ofs << "  <dem>" << demIt->second << "</dem>\n";
            }

            for (const auto& [key, value] : params)
            {
                if (key != "master" && key != "slave" && key != "dem" && key != "output")
                {
                    ofs << "  <" << key << ">" << value << "</" << key << ">\n";
                }
            }

            ofs << "</config>\n";
            ofs.close();

            return true;
        }
        catch (const std::exception& e)
        {
            logger_->error("Failed to write topsApp config: {}", e.what());
            return false;
        }
    }

}  // namespace soilwatercm::infrastructure
