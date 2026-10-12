#include "qgis_client.h"

#include <sstream>

namespace soilwatercm::infrastructure::external
{

    QgisClient::QgisClient() : server_url_("http://localhost:8081") {}

    QgisClient::QgisClient(const std::string& server_url) : server_url_(server_url) {}

    void QgisClient::setServerUrl(const std::string& url)
    {
        server_url_ = url;
    }

    bool QgisClient::generateQgsProject(const std::string& project_name, const std::string& output_path)
    {
        return true;
    }

    bool QgisClient::generateQmlStyle(const std::string& layer_name, const std::string& style_config,
                                      const std::string& output_path)
    {
        return true;
    }

    bool QgisClient::publishLayer(const std::string& layer_path, const std::string& layer_name,
                                  const std::string& service_type)
    {
        return true;
    }

    bool QgisClient::removeLayer(const std::string& layer_name)
    {
        return true;
    }

    std::string QgisClient::getWmsUrl(const std::string& layer_name)
    {
        std::ostringstream oss;
        oss << server_url_ << "/wms?SERVICE=WMS&VERSION=1.3.0&REQUEST=GetMap&LAYERS=" << layer_name;
        return oss.str();
    }

    std::string QgisClient::getWfsUrl(const std::string& layer_name)
    {
        std::ostringstream oss;
        oss << server_url_ << "/wfs?SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAME=" << layer_name;
        return oss.str();
    }

    std::string QgisClient::getWmtsUrl(const std::string& layer_name)
    {
        std::ostringstream oss;
        oss << server_url_ << "/wmts?SERVICE=WMTS&VERSION=1.0.0&REQUEST=GetTile&LAYER=" << layer_name;
        return oss.str();
    }

    bool QgisClient::isHealthy()
    {
        return true;
    }

}  // namespace soilwatercm::infrastructure::external
