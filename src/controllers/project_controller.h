#pragma once

#include <drogon/HttpController.h>

#include "../services/project_service.h"

namespace soilwatercm::controllers
{

    /**
     * @brief 项目 HTTP 控制器
     *
     * 提供项目 CRUD 与边界/资料管理, 路由前缀 /api/v1/projects。
     */
    class ProjectController : public drogon::HttpController<ProjectController, false>
    {
    public:
        ProjectController() = default;

        /**
         * @brief 构造, 注入项目服务
         * @param projectService 项目服务
         */
        explicit ProjectController(std::shared_ptr<services::ProjectService> projectService);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(ProjectController::listProjects, "/api/v1/projects", drogon::Get);
        ADD_METHOD_TO(ProjectController::createProject, "/api/v1/projects", drogon::Post);
        ADD_METHOD_TO(ProjectController::getProject, "/api/v1/projects/{id}", drogon::Get);
        ADD_METHOD_TO(ProjectController::updateProject, "/api/v1/projects/{id}", drogon::Put);
        ADD_METHOD_TO(ProjectController::deleteProject, "/api/v1/projects/{id}", drogon::Delete);
        ADD_METHOD_TO(ProjectController::getPeriods, "/api/v1/projects/{id}/periods", drogon::Get);
        ADD_METHOD_TO(ProjectController::getBoundary, "/api/v1/projects/{id}/boundary", drogon::Get);
        ADD_METHOD_TO(ProjectController::downloadBoundary, "/api/v1/projects/{id}/boundary/download", drogon::Get);
        ADD_METHOD_TO(ProjectController::uploadBoundary, "/api/v1/projects/{id}/boundary/upload", drogon::Post);
        ADD_METHOD_TO(ProjectController::listDocuments, "/api/v1/projects/{id}/documents", drogon::Get);
        ADD_METHOD_TO(ProjectController::uploadDocument, "/api/v1/projects/{id}/documents", drogon::Post);
        ADD_METHOD_TO(ProjectController::downloadDocument, "/api/v1/projects/{id}/documents/{doc_id}/download",
                     drogon::Get);
        ADD_METHOD_TO(ProjectController::deleteDocument, "/api/v1/projects/{id}/documents/{doc_id}", drogon::Delete);
        METHOD_LIST_END

        /**
         * @brief GET /api/v1/projects 列出全部项目
         * @return 通过 callback 返回项目列表
         */
        void listProjects(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/projects 创建项目
         * @return 通过 callback 返回 201 与新记录
         */
        void createProject(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/projects/{id} 查询项目详情
         * @param id 项目 id
         * @return 通过 callback 返回项目记录
         */
        void getProject(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief PUT /api/v1/projects/{id} 更新项目信息
         * @param id 项目 id
         * @return 通过 callback 返回更新后的记录
         */
        void updateProject(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief DELETE /api/v1/projects/{id} 删除项目
         * @param id 项目 id
         * @return 通过 callback 返回 204
         */
        void deleteProject(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief GET /api/v1/projects/{id}/periods 列出项目下监测期
         * @param id 项目 id
         * @return 通过 callback 返回监测期列表
         */
        void getPeriods(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief GET /api/v1/projects/{id}/boundary 查询项目边界
         * @param id 项目 id
         * @return 通过 callback 返回防治责任范围边界 (GeoJSON)
         */
        void getBoundary(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief GET /api/v1/projects/{id}/boundary/download 下载边界文件
         * @param id 项目 id
         * @return 通过 callback 返回边界 GeoJSON 附件
         */
        void downloadBoundary(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief POST /api/v1/projects/{id}/boundary/upload 上传边界文件
         * @param id 项目 id
         * @return 通过 callback 返回上传结果; 支持 shp 多文件/geojson, 坐标系取 X-EPSG 头
         */
        void uploadBoundary(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief GET /api/v1/projects/{id}/documents 列出项目资料
         * @param id 项目 id
         * @return 通过 callback 返回资料列表
         */
        void listDocuments(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief POST /api/v1/projects/{id}/documents 上传项目资料文件
         * @param id 项目 id
         * @return 通过 callback 返回 201 与资料记录
         */
        void uploadDocument(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief GET /api/v1/projects/{id}/documents/{doc_id}/download 下载资料文件
         * @param id 项目 id
         * @param docId 资料 id
         * @return 通过 callback 返回资料文件
         */
        void downloadDocument(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id, int docId);

        /**
         * @brief DELETE /api/v1/projects/{id}/documents/{doc_id} 删除资料
         * @param id 项目 id
         * @param docId 资料 id
         * @return 通过 callback 返回 204
         */
        void deleteDocument(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id, int docId);

    private:
        std::shared_ptr<services::ProjectService> projectService_;
    };

}  // namespace soilwatercm::controllers
