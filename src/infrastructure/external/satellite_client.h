#pragma once

#include <memory>
#include <string>
#include <vector>

namespace soilwatercm::infrastructure::external
{

    /// 卫星影像元数据
    struct SatelliteMetadata
    {
        std::string satellite_name;
        std::string sensor;
        std::string acquisition_time;
        std::string orbit;
        double cloud_cover;
        std::string projection;
        std::vector<double> bbox;
    };

    /**
     * @brief 卫星影像数据源客户端 (Sentinel/Landsat/CRESDA)
     *
     * 当前各方法均为桩实现, 只记日志返回空。
     */
    class SatelliteClient
    {
    public:
        SatelliteClient();
        ~SatelliteClient() = default;

        /**
         * @brief 设置数据源凭据
         * @param api_key API key
         * @param secret API secret, 可空
         */
        void setCredentials(const std::string& api_key, const std::string& secret = "");

        /**
         * @brief 按范围 + 时间窗检索 Sentinel 影像 (桩实现, 只记日志返回空)
         * @param bbox 范围
         * @param start_date 起始日期
         * @param end_date 结束日期
         * @param max_cloud_cover 最大云量比例
         * @return 影像 id 列表
         */
        std::vector<std::string> searchSentinel(const std::string& bbox, const std::string& start_date,
                                                const std::string& end_date, double max_cloud_cover = 0.1);

        /// @brief 按范围 + 时间窗检索 Landsat 影像 (桩实现, 只记日志返回空)
        std::vector<std::string> searchLandsat(const std::string& bbox, const std::string& start_date,
                                               const std::string& end_date, double max_cloud_cover = 0.1);

        /// @brief 按范围 + 时间窗检索 CRESDA 国产影像 (桩实现, 只记日志返回空)
        std::vector<std::string> searchCresda(const std::string& bbox, const std::string& start_date,
                                              const std::string& end_date);

        /// @brief 下载影像 (桩实现, 只记日志返回空串)
        std::string downloadImage(const std::string& image_id, const std::string& output_dir);

        /// @brief 下载快视图 (桩实现, 只记日志返回空串)
        std::string downloadQuicklook(const std::string& image_id, const std::string& output_dir);

        /// @brief 查影像元数据 (桩实现, 返回 satellite_name=Unknown 的空壳)
        SatelliteMetadata getMetadata(const std::string& image_id);

        /// @brief 服务健康检查 (桩实现, 恒返回 true)
        bool isHealthy();

        SatelliteClient(const SatelliteClient&) = delete;
        SatelliteClient& operator=(const SatelliteClient&) = delete;
        SatelliteClient(SatelliteClient&&) = default;
        SatelliteClient& operator=(SatelliteClient&&) = default;

    private:
        std::string api_key_;
        std::string secret_;
    };

}  // namespace soilwatercm::infrastructure::external
