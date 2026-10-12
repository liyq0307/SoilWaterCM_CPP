#include "satellite_client.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <regex>
#include <sstream>
#include <thread>

#include "../../utils/logger.h"

namespace soilwatercm::infrastructure::external
{

    SatelliteClient::SatelliteClient() {}

    void SatelliteClient::setCredentials(const std::string& api_key, const std::string& secret)
    {
        api_key_ = api_key;
        secret_ = secret;
    }

    std::vector<std::string> SatelliteClient::searchSentinel(const std::string& bbox, const std::string& start_date,
                                                             const std::string& end_date, double max_cloud_cover)
    {
        utils::Logger::info("Satellite: searching Sentinel for bbox={} dates={}-{} cloud<={}%", bbox, start_date,
                            end_date, static_cast<int>(max_cloud_cover * 100));
        return {};
    }

    std::vector<std::string> SatelliteClient::searchLandsat(const std::string& bbox, const std::string& start_date,
                                                            const std::string& end_date, double max_cloud_cover)
    {
        utils::Logger::info("Satellite: searching Landsat for bbox={} dates={}-{} cloud<={}%", bbox, start_date,
                            end_date, static_cast<int>(max_cloud_cover * 100));
        return {};
    }

    std::vector<std::string> SatelliteClient::searchCresda(const std::string& bbox, const std::string& start_date,
                                                           const std::string& end_date)
    {
        utils::Logger::info("Satellite: searching CRESDA for bbox={} dates={}-{}", bbox, start_date, end_date);
        return {};
    }

    std::string SatelliteClient::downloadImage(const std::string& image_id, const std::string& output_dir)
    {
        utils::Logger::info("Satellite: starting download of {}", image_id);
        return "";
    }

    std::string SatelliteClient::downloadQuicklook(const std::string& image_id, const std::string& output_dir)
    {
        utils::Logger::info("Satellite: downloading quicklook for {}", image_id);
        return "";
    }

    SatelliteMetadata SatelliteClient::getMetadata(const std::string& image_id)
    {
        SatelliteMetadata metadata;
        metadata.satellite_name = "Unknown";
        metadata.cloud_cover = 0.0;
        utils::Logger::info("Satellite: fetching metadata for {}", image_id);
        return metadata;
    }

    bool SatelliteClient::isHealthy()
    {
        return true;
    }

}  // namespace soilwatercm::infrastructure::external
