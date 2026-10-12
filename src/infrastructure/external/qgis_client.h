#pragma once

#include <memory>
#include <string>

namespace soilwatercm::infrastructure::external
{

    /**
     * @brief QGIS Server 客户端
     *
     * 按服务地址拼 WMS/WFS/WMTS 取图 URL。项目生成与图层发布当前是桩实现, 恒返回成功。
     */
    class QgisClient
    {
    public:
        /// @brief 构造, 服务地址默认 http://localhost:8081
        QgisClient();

        /**
         * @brief 构造, 指定 QGIS Server 地址
         * @param server_url 服务基地址
         */
        explicit QgisClient(const std::string& server_url);
        ~QgisClient() = default;

        /// @brief 设置 QGIS Server 地址
        void setServerUrl(const std::string& url);

        /// @brief 生成 QGS 工程文件 (桩实现, 恒返回 true)
        bool generateQgsProject(const std::string& project_name, const std::string& output_path);

        /// @brief 生成 QML 样式文件 (桩实现, 恒返回 true)
        bool generateQmlStyle(const std::string& layer_name, const std::string& style_config,
                              const std::string& output_path);

        /// @brief 发布图层为 OGC 服务 (桩实现, 恒返回 true)
        bool publishLayer(const std::string& layer_path, const std::string& layer_name,
                          const std::string& service_type = "WMS");

        /// @brief 下架图层 (桩实现, 恒返回 true)
        bool removeLayer(const std::string& layer_name);

        /// @brief 拼出 WMS GetMap 地址 (WMS 1.3.0)
        std::string getWmsUrl(const std::string& layer_name);

        /// @brief 拼出 WFS GetFeature 地址 (WFS 2.0.0)
        std::string getWfsUrl(const std::string& layer_name);

        /// @brief 拼出 WMTS GetTile 地址 (WMTS 1.0.0)
        std::string getWmtsUrl(const std::string& layer_name);

        /// @brief 服务健康检查 (桩实现, 恒返回 true)
        bool isHealthy();

        QgisClient(const QgisClient&) = delete;
        QgisClient& operator=(const QgisClient&) = delete;
        QgisClient(QgisClient&&) = default;
        QgisClient& operator=(QgisClient&&) = default;

    private:
        std::string server_url_;
    };

}  // namespace soilwatercm::infrastructure::external
