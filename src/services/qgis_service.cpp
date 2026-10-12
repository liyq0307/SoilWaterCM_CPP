#include "qgis_service.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "../infrastructure/config_manager.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{

    QgisService::QgisService(std::shared_ptr<infrastructure::external::QgisClient> qgisClient,
                             std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal)
        : qgisClient_(std::move(qgisClient)), gdal_(std::move(gdal))
    {
    }

    bool QgisService::publishLayer(const PublishLayerParams& params)
    {
        std::string qgsPath =
            generateQgsProject(params.layer_name, {params.data_path}, "qgis/projects/" + params.layer_name + ".qgs");

        std::string qmlPath =
            generateQmlStyle(params.layer_name, params.style_name, "qgis/styles/" + params.layer_name + ".qml");

        bool success = qgisClient_->publishLayer(qgsPath, params.layer_name);

        if (success)
        {
            utils::Logger::info("Layer published: {}", params.layer_name);
        }

        return success;
    }

    std::string QgisService::generateQgsProject(const std::string& projectName,
                                                const std::vector<std::string>& layerPaths,
                                                const std::string& outputPath)
    {
        std::filesystem::create_directories(std::filesystem::path(outputPath).parent_path());

        std::string qgsContent = R"(<!DOCTYPE qgis PUBLIC 'http://mrcc.com/qgis.dtd' 'SYSTEM'>
<QGIS version="3.34">
  <title>)" + projectName + R"(</title>
  <projectCrs>
    <spatialrefsys>
      <authid>EPSG:4326</authid>
    </spatialrefsys>
  </projectCrs>
  <projectlayers>
)";

        for (size_t i = 0; i < layerPaths.size(); ++i)
        {
            qgsContent += R"(    <maplayer>
      <id>layer_)" + std::to_string(i) +
                          R"(</id>
      <name>Layer )" + std::to_string(i) +
                          R"(</name>
      <datasource>)" + layerPaths[i] +
                          R"(</datasource>
      <provider>ogr</provider>
    </maplayer>
)";
        }

        qgsContent += R"(  </projectlayers>
</QGIS>
)";

        std::ofstream file(outputPath);
        file << qgsContent;
        file.close();

        utils::Logger::info("QGS project generated: {}", outputPath);

        return outputPath;
    }

    std::string QgisService::generateQmlStyle(const std::string& /*layerName*/, const std::string& /*styleType*/,
                                              const std::string& outputPath)
    {
        std::filesystem::create_directories(std::filesystem::path(outputPath).parent_path());

        std::string qmlContent = R"(<!DOCTYPE qgis PUBLIC 'http://mrcc.com/qgis.dtd' 'SYSTEM'>
<qgis version="3.34">
  <renderer-v2 type="singleSymbol">
    <symbol name="default" type="fill">
      <layer pass="0" locked="0">
        <prop k="color" v="128,128,128,255"/>
      </layer>
    </symbol>
  </renderer-v2>
</qgis>
)";

        std::ofstream file(outputPath);
        file << qmlContent;
        file.close();

        utils::Logger::info("QML style generated: {}", outputPath);

        return outputPath;
    }

    std::vector<QgsProjectInfo> QgisService::listServices()
    {
        std::vector<QgsProjectInfo> services;

        std::string projectsDir = "qgis/projects";

        if (!std::filesystem::exists(projectsDir))
        {
            return services;
        }

        for (const auto& entry : std::filesystem::directory_iterator(projectsDir))
        {
            if (entry.path().extension() == ".qgs")
            {
                QgsProjectInfo info;
                info.name = entry.path().stem().string();
                info.path = entry.path().string();
                services.push_back(info);
            }
        }

        return services;
    }

    bool QgisService::deleteService(const std::string& serviceName)
    {
        std::string qgsPath = "qgis/projects/" + serviceName + ".qgs";

        if (!std::filesystem::exists(qgsPath))
        {
            return false;
        }

        std::filesystem::remove(qgsPath);

        utils::Logger::info("Service deleted: {}", serviceName);

        return true;
    }

    std::optional<QgsProjectInfo> QgisService::getServiceInfo(const std::string& serviceName)
    {
        std::string qgsPath = "qgis/projects/" + serviceName + ".qgs";

        if (!std::filesystem::exists(qgsPath))
        {
            return std::nullopt;
        }

        QgsProjectInfo info;
        info.name = serviceName;
        info.path = qgsPath;

        return info;
    }

    namespace
    {
        // 对齐 Python qgis_publish._service_name: project_{id}[_p{period}]
        std::string serviceFileName(int projectId, int periodId)
        {
            std::ostringstream oss;
            oss << "project_" << projectId;
            if (periodId > 0)
            {
                oss << "_p" << periodId;
            }
            return oss.str();
        }

        std::string qgisProjectsDir()
        {
            auto& config = infrastructure::ConfigManager::getInstance();
            return config.get<std::string>("qgis.projects_dir", "qgis/projects");
        }
    }  // namespace

    bool QgisService::unpublishProject(int projectId, int periodId)
    {
        std::string name = serviceFileName(projectId, periodId);
        std::string qgsPath = qgisProjectsDir() + "/" + name + ".qgs";

        if (!std::filesystem::exists(qgsPath))
        {
            utils::Logger::warn("Unpublish failed, service not found: {}", qgsPath);
            return false;
        }

        std::error_code ec;
        std::filesystem::remove(qgsPath, ec);
        std::filesystem::remove(qgisProjectsDir() + "/" + name + ".qgs~", ec);
        std::filesystem::remove("qgis/styles/" + name + ".qml", ec);

        qgisClient_->removeLayer(name);

        utils::Logger::info("Project unpublished: {}", name);

        return true;
    }

    std::optional<QgisWmsInfo> QgisService::findWmsInfo(int projectId, int periodId)
    {
        std::string projectsDir = qgisProjectsDir();
        std::string expectedName = serviceFileName(projectId, periodId);
        std::string expectedPath = projectsDir + "/" + expectedName + ".qgs";

        std::string hitPath;
        bool fallback = false;

        if (std::filesystem::exists(expectedPath))
        {
            hitPath = expectedPath;
        }
        else
        {
            // 回退: 该项目最新发布的工程 (对齐 Python find_project_qgs)
            std::string prefix = "project_" + std::to_string(projectId);
            std::filesystem::path latest;
            auto latestTime = std::filesystem::file_time_type::min();

            if (std::filesystem::exists(projectsDir))
            {
                for (const auto& entry : std::filesystem::directory_iterator(projectsDir))
                {
                    if (entry.path().extension() != ".qgs")
                        continue;
                    std::string stem = entry.path().stem().string();
                    if (stem.rfind(prefix, 0) != 0)
                        continue;
                    auto mtime = std::filesystem::last_write_time(entry.path());
                    if (mtime > latestTime)
                    {
                        latestTime = mtime;
                        latest = entry.path();
                    }
                }
            }

            if (latest.empty())
            {
                return std::nullopt;
            }

            hitPath = latest.string();
            expectedName = latest.stem().string();
            fallback = periodId > 0;
        }

        auto& config = infrastructure::ConfigManager::getInstance();
        std::string serverUrl = config.get<std::string>("qgis.server_url", "http://localhost:8081");

        QgisWmsInfo info;
        info.qgsPath = hitPath;
        info.service = expectedName;
        info.fallback = fallback;
        info.wmsBase = serverUrl + "/qgis/server?MAP=" + hitPath;
        info.getCapabilitiesUrl = info.wmsBase + "&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";

        return info;
    }

}  // namespace soilwatercm::services
