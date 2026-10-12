#include "saga_interface.h"

#include <future>
#include <sstream>

#include "../../utils/logger.h"

namespace soilwatercm::infrastructure
{

    SagaInterface::SagaInterface(std::shared_ptr<QgisClient> qgisClient)
        : qgis_client_(qgisClient), logger_(utils::Logger::getLogger("saga_interface"))
    {
        if (qgis_client_)
        {
            available_ = true;
            logger_->info("SAGA GIS interface initialized via QGIS");
        }
        else
        {
            available_ = false;
            logger_->warn("QGIS client is null, SAGA GIS interface disabled");
        }
    }

    ToolResult SagaInterface::execute(const std::string& algorithm, const std::map<std::string, std::string>& params,
                                      const ProgressCallback& callback)
    {
        if (!available_)
        {
            return {false, "", "SAGA GIS interface not available", -1, {}};
        }

        logger_->info("Executing SAGA algorithm: {}", algorithm);

        logger_->warn("SAGA execution requires QGIS processing backend, which is not available in this build");
        return {false, "", "SAGA execution not available without QGIS processing", -1, {}};
    }

    std::future<ToolResult> SagaInterface::executeAsync(const std::string& algorithm,
                                                        const std::map<std::string, std::string>& params,
                                                        const ProgressCallback& callback)
    {
        return std::async(std::launch::async,
                          [this, algorithm, params, callback]() { return execute(algorithm, params, callback); });
    }

    ToolResult SagaInterface::analyzeTerrainMorphology(const std::string& demPath, const std::string& output)
    {
        std::map<std::string, std::string> params = {{"DEM", demPath}, {"RESULT", output}};

        return execute("ta_morphometry", params);
    }

    ToolResult SagaInterface::calculateKFactor(const std::string& soilDataPath, const std::string& output)
    {
        std::map<std::string, std::string> params = {{"SOIL", soilDataPath}, {"K_FACTOR", output}};

        return execute("sim_qm_of_erodibility", params);
    }

    ToolResult SagaInterface::calculateTWI(const std::string& demPath, const std::string& output)
    {
        std::map<std::string, std::string> params = {{"DEM", demPath}, {"TWI", output}};

        return execute("ta_hydrology_topographicwetnessindex", params);
    }

    ToolResult SagaInterface::calculateConvergenceIndex(const std::string& demPath, const std::string& output)
    {
        std::map<std::string, std::string> params = {{"DEM", demPath}, {"RESULT", output}};

        return execute("ta_morphometry_convergenceindex", params);
    }

    ToolResult SagaInterface::calculateLSFactor(const std::string& demPath, const std::string& output)
    {
        std::map<std::string, std::string> params = {
            {"DEM", demPath}, {"SLOPE", output + "_slope"}, {"AREA", output + "_area"}, {"LS", output}};

        return execute("ta_morphometry", params);
    }

    std::vector<AlgorithmInfo> SagaInterface::listAlgorithms()
    {
        return {{"ta_morphometry", "Terrain Morphometry Analysis", "", {"DEM", "RESULT"}},
                {"sim_qm_of_erodibility", "Soil Erodibility K-Factor", "", {"SOIL", "K_FACTOR"}},
                {"ta_hydrology_topographicwetnessindex", "Topographic Wetness Index", "", {"DEM", "TWI"}},
                {"ta_morphometry_convergenceindex", "Convergence Index", "", {"DEM", "RESULT"}},
                {"grid_calculus", "Grid Calculus", "", {"GRIDS", "RESULT", "FORMULA"}},
                {"grid_statistics", "Grid Statistics", "", {"GRID", "STATISTICS"}}};
    }

    bool SagaInterface::isAvailable()
    {
        return available_;
    }

    std::string SagaInterface::getVersion()
    {
        return version_;
    }

    std::string SagaInterface::buildProcessingParams(const std::map<std::string, std::string>& params)
    {
        std::ostringstream oss;
        bool first = true;

        for (const auto& [key, value] : params)
        {
            if (!first)
            {
                oss << "|";
            }
            oss << key << "=" << value;
            first = false;
        }

        return oss.str();
    }

    std::string SagaInterface::extractOutputFromResult(const std::string& result)
    {
        size_t pos = result.find("OUTPUT:");
        if (pos != std::string::npos)
        {
            return result.substr(pos + 7);
        }
        return "";
    }

}  // namespace soilwatercm::infrastructure
