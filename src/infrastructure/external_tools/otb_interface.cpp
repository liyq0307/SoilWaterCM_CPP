#include "otb_interface.h"

#include <filesystem>
#include <future>
#include <sstream>

#include "utils/logger.h"

namespace soilwatercm::infrastructure
{

    namespace fs = std::filesystem;

    OtbInterface::OtbInterface(const std::string& otbRootPath, OtbCallMethod method)
        : otb_root_path_(otbRootPath),
          call_method_(method),
          process_manager_(std::make_unique<ProcessManager>()),
          logger_(utils::Logger::getLogger("otb_interface"))
    {
        available_ = detectOtbInstallation();

        if (available_)
        {
            logger_->info("OTB interface initialized, version: {}, root: {}", version_, otb_root_path_);
        }
        else
        {
            logger_->warn("OTB not found at: {}, interface disabled", otb_root_path_);
        }
    }

    ToolResult OtbInterface::execute(const std::string& algorithm, const std::map<std::string, std::string>& params,
                                     const ProgressCallback& callback)
    {
        if (!available_)
        {
            return {false, "", "OTB interface not available", -1, {}};
        }

        logger_->info("Executing OTB algorithm: {}", algorithm);

        if (call_method_ == OtbCallMethod::CommandLine)
        {
            return executeCommandLine(algorithm, params, callback);
        }
        else
        {
            logger_->error("OTB C++ API not implemented yet");
            return {false, "", "OTB C++ API not implemented", -1, {}};
        }
    }

    std::future<ToolResult> OtbInterface::executeAsync(const std::string& algorithm,
                                                       const std::map<std::string, std::string>& params,
                                                       const ProgressCallback& callback)
    {
        return std::async(std::launch::async,
                          [this, algorithm, params, callback]() { return execute(algorithm, params, callback); });
    }

    ToolResult OtbInterface::classifyImage(const std::string& imagePath, const std::string& modelPath,
                                           const std::string& output)
    {
        std::map<std::string, std::string> params = {{"in", imagePath}, {"model", modelPath}, {"out", output}};

        return execute("ImageClassifier", params);
    }

    ToolResult OtbInterface::detectChange(const std::string& beforeImage, const std::string& afterImage,
                                          const std::string& output)
    {
        std::map<std::string, std::string> params = {{"in1", beforeImage}, {"in2", afterImage}, {"out", output}};

        return execute("MultivariateAlterationDetector", params);
    }

    ToolResult OtbInterface::segmentImage(const std::string& imagePath, const std::string& output,
                                          double spatialThreshold, double spectralThreshold)
    {
        std::map<std::string, std::string> params = {{"in", imagePath},
                                                     {"out", output},
                                                     {"spatial.threshold", std::to_string(spatialThreshold)},
                                                     {"spectral.threshold", std::to_string(spectralThreshold)}};

        return execute("LSMSSegmentation", params);
    }

    ToolResult OtbInterface::extractFeatures(const std::string& imagePath, const std::string& output,
                                             const std::vector<std::string>& features)
    {
        std::map<std::string, std::string> params = {{"in", imagePath}, {"out", output}};

        std::ostringstream oss;
        for (size_t i = 0; i < features.size(); ++i)
        {
            if (i > 0)
                oss << ",";
            oss << features[i];
        }
        params["features"] = oss.str();

        return execute("Harmonization", params);
    }

    ToolResult OtbInterface::opticalCalibration(const std::string& inputPath, const std::string& outputPath)
    {
        std::map<std::string, std::string> params = {{"in", inputPath}, {"out", outputPath}, {"output", "image"}};
        return execute("OpticalCalibration", params);
    }

    ToolResult OtbInterface::orthoRectification(const std::string& inputPath, const std::string& outputPath,
                                                const std::string& demPath)
    {
        std::map<std::string, std::string> params = {{"in", inputPath}, {"out", outputPath}, {"output", "image"}};
        if (!demPath.empty())
        {
            params["dem"] = demPath;
            params["dem.grid.resample"] = "bilinear";
        }
        return execute("OrthoRectification", params);
    }

    ToolResult OtbInterface::changeDetection(const std::string& beforePath, const std::string& afterPath,
                                             const std::string& outputPath)
    {
        std::map<std::string, std::string> params = {{"in1", beforePath}, {"in2", afterPath}, {"out", outputPath}};
        return execute("DifferenceImage", params);
    }

    std::vector<AlgorithmInfo> OtbInterface::listAlgorithms()
    {
        return {{"ImageClassifier", "Image Classification", "", {"in", "model", "out"}},
                {"MultivariateAlterationDetector", "Change Detection", "", {"in1", "in2", "out"}},
                {"LSMSSegmentation", "LSMS Segmentation", "", {"in", "out", "spatial.threshold", "spectral.threshold"}},
                {"BandMath", "Band Math", "", {"il", "out", "exp"}},
                {"Resample", "Image Resampling", "", {"in", "out", "sizex", "sizey"}},
                {"ConcatenateImages", "Concatenate Images", "", {"il", "out"}},
                {"ComputeImagesStatistics", "Compute Statistics", "", {"il", "out"}},
                {"TrainImageClassifier", "Train Classifier", "", {"io.il", "io.vd", "io.out"}}};
    }

    bool OtbInterface::isAvailable()
    {
        return available_;
    }

    std::string OtbInterface::getVersion()
    {
        return version_;
    }

    ToolResult OtbInterface::executeCommandLine(const std::string& algorithm,
                                                const std::map<std::string, std::string>& params,
                                                const ProgressCallback& callback)
    {
        std::string command = buildCommandLine(algorithm, params);

        ProcessResult result = process_manager_->execute(command, std::vector<std::string>{}, "");

        std::string outputFile;
        auto it = params.find("out");
        if (it != params.end())
        {
            outputFile = it->second;
        }

        return {result.exit_code == 0, result.output, result.error, result.exit_code, {{"output", outputFile}}};
    }

    std::string OtbInterface::buildCommandLine(const std::string& algorithm,
                                               const std::map<std::string, std::string>& params)
    {
        std::ostringstream oss;

        std::string otbcli = findOtbcliPath();
        oss << otbcli << algorithm;

        for (const auto& [key, value] : params)
        {
            oss << " -" << key << " " << value;
        }

        return oss.str();
    }

    bool OtbInterface::detectOtbInstallation()
    {
        std::string otbcli = findOtbcliPath();

        if (otbcli.empty())
        {
            return false;
        }

        ProcessResult result = process_manager_->execute(otbcli + " -help", std::vector<std::string>{}, "");

        if (result.exit_code == 0)
        {
            std::string output = result.output;
            size_t pos = output.find("OTB version:");
            if (pos != std::string::npos)
            {
                size_t start = pos + 12;
                size_t end = output.find('\n', start);
                if (end != std::string::npos)
                {
                    version_ = output.substr(start, end - start);
                    size_t first = version_.find_first_not_of(" \t");
                    size_t last = version_.find_last_not_of(" \t");
                    if (first != std::string::npos && last != std::string::npos)
                    {
                        version_ = version_.substr(first, last - first + 1);
                    }
                }
            }
            return true;
        }

        return false;
    }

    std::string OtbInterface::findOtbcliPath()
    {
        fs::path otbcliPath;

#ifdef _WIN32
        otbcliPath = fs::path(otb_root_path_) / "bin" / "otbcli.bat";
#else
        otbcliPath = fs::path(otb_root_path_) / "bin" / "otbcli";
#endif

        if (fs::exists(otbcliPath))
        {
            return otbcliPath.string();
        }

        otbcliPath = fs::path(otb_root_path_) / "lib" / "otb" / "applications" / "otbcli";
        if (fs::exists(otbcliPath))
        {
            return otbcliPath.string();
        }

        return "";
    }

}  // namespace soilwatercm::infrastructure
