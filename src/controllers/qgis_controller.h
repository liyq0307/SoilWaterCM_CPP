#pragma once

#include <drogon/HttpController.h>

#include "../services/qgis_service.h"

namespace soilwatercm::controllers
{

    /**
     * @brief QGIS 地图服务发布 HTTP 控制器
     *
     * 提供图层发布/下线与 WMS 服务查询, 路由前缀 /api/v1/qgis。
     */
    class QgisController : public drogon::HttpController<QgisController, false>
    {
    public:
        QgisController() = default;

        /**
         * @brief 构造, 注入 QGIS 服务
         * @param qgisService 地图服务发布服务
         */
        explicit QgisController(std::shared_ptr<services::QgisService> qgisService);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(QgisController::publish, "/api/v1/qgis/publish", drogon::Post);
        ADD_METHOD_TO(QgisController::unpublish, "/api/v1/qgis/publish", drogon::Delete);
        ADD_METHOD_TO(QgisController::wmsUrl, "/api/v1/qgis/wms-url", drogon::Get);
        ADD_METHOD_TO(QgisController::listServices, "/api/v1/qgis/services", drogon::Get);
        ADD_METHOD_TO(QgisController::getService, "/api/v1/qgis/services/{name}", drogon::Get);
        ADD_METHOD_TO(QgisController::deleteService, "/api/v1/qgis/services/{name}", drogon::Delete);
        METHOD_LIST_END

        /**
         * @brief POST /api/v1/qgis/publish 发布图层为 QGIS 地图服务
         * @return 通过 callback 返回发布结果
         */
        void publish(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief DELETE /api/v1/qgis/publish?project_id=&period_id= 下线项目地图服务
         * @return 通过 callback 返回 204
         */
        void unpublish(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/qgis/wms-url?project_id=&period_id= 查询 WMS 服务地址
         * @return 通过 callback 返回项目 WMS 服务地址信息
         */
        void wmsUrl(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/qgis/services 列出已发布地图服务
         * @return 通过 callback 返回服务列表
         */
        void listServices(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/qgis/services/{name} 查询单个服务详情
         * @param name 服务名称
         * @return 通过 callback 返回服务详情
         */
        void getService(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& name);

        /**
         * @brief DELETE /api/v1/qgis/services/{name} 删除服务
         * @param name 服务名称
         * @return 通过 callback 返回 204
         */
        void deleteService(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& name);

    private:
        std::shared_ptr<services::QgisService> qgisService_;
    };

}  // namespace soilwatercm::controllers
