#pragma once

#include <drogon/HttpController.h>

#include "../services/basemap_service.h"

namespace soilwatercm::controllers
{

    /**
     * @brief 正射影像底图 HTTP 控制器
     *
     * 提供底图列表、上传与删除, 路由前缀 /api/v1/basemaps。
     */
    class BasemapController : public drogon::HttpController<BasemapController, false>
    {
    public:
        BasemapController() = default;

        /**
         * @brief 构造, 注入底图服务
         * @param basemapService 正射影像底图服务
         */
        explicit BasemapController(std::shared_ptr<services::BasemapService> basemapService);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(BasemapController::listBasemaps, "/api/v1/basemaps", drogon::Get);
        ADD_METHOD_TO(BasemapController::uploadBasemap, "/api/v1/basemaps/upload", drogon::Post);
        ADD_METHOD_TO(BasemapController::deleteBasemap, "/api/v1/basemaps/{id}", drogon::Delete);
        METHOD_LIST_END

        /**
         * @brief GET /api/v1/basemaps?project_id= 列出底图
         * @return 通过 callback 返回底图列表; project_id 为空则返回全部
         */
        void listBasemaps(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/basemaps/upload 上传底图影像
         *
         * 主文件与附属文件一并上传。
         * @return 通过 callback 返回 201 与底图记录
         */
        void uploadBasemap(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief DELETE /api/v1/basemaps/{id} 删除底图
         * @param id 底图 id
         * @return 通过 callback 返回 204
         */
        void deleteBasemap(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

    private:
        std::shared_ptr<services::BasemapService> basemapService_;
    };

}  // namespace soilwatercm::controllers
