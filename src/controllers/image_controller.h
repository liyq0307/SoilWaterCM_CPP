#pragma once

#include <drogon/HttpController.h>

#include "../services/image_service.h"

namespace soilwatercm::controllers
{

    /**
     * @brief 影像数据管理 HTTP 控制器
     *
     * 提供上传/分片上传/路径注册/下载/质检, 路由前缀 /api/v1/images。
     */
    class ImageController : public drogon::HttpController<ImageController, false>
    {
    public:
        ImageController() = default;

        /**
         * @brief 构造, 注入影像服务
         * @param imageService 影像数据服务
         */
        explicit ImageController(std::shared_ptr<services::ImageService> imageService);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(ImageController::listImages, "/api/v1/images", drogon::Get);
        ADD_METHOD_TO(ImageController::uploadImage, "/api/v1/images", drogon::Post);
        ADD_METHOD_TO(ImageController::chunkInit, "/api/v1/images/chunk/init", drogon::Post);
        ADD_METHOD_TO(ImageController::uploadChunk, "/api/v1/images/upload/chunk", drogon::Post);
        ADD_METHOD_TO(ImageController::mergeChunks, "/api/v1/images/upload/merge", drogon::Post);
        ADD_METHOD_TO(ImageController::uploadFolder, "/api/v1/images/upload-folder", drogon::Post);
        ADD_METHOD_TO(ImageController::registerPath, "/api/v1/images/register-path", drogon::Post);
        ADD_METHOD_TO(ImageController::registerFolder, "/api/v1/images/register-folder", drogon::Post);
        ADD_METHOD_TO(ImageController::registerOblique, "/api/v1/images/register-oblique", drogon::Post);
        ADD_METHOD_TO(ImageController::getObliqueTileset, "/api/v1/images/oblique-tileset", drogon::Get);
        ADD_METHOD_TO(ImageController::getImage, "/api/v1/images/{id}", drogon::Get);
        ADD_METHOD_TO(ImageController::deleteImage, "/api/v1/images/{id}", drogon::Delete);
        ADD_METHOD_TO(ImageController::downloadImage, "/api/v1/images/{id}/download", drogon::Get);
        ADD_METHOD_TO(ImageController::qualityCheck, "/api/v1/images/{id}/quality-check", drogon::Post);
        ADD_METHOD_TO(ImageController::deletePreprocess, "/api/v1/images/{id}/preprocess", drogon::Delete);
        METHOD_LIST_END

        /**
         * @brief GET /api/v1/images?project_id= 按项目列出影像
         * @return 通过 callback 返回影像列表
         */
        void listImages(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/images 上传单个影像文件
         * @return 通过 callback 返回 201 与影像记录; multipart 表单
         */
        void uploadImage(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/images/chunk/init 初始化分片上传
         * @return 通过 callback 返回 upload_id 与分片大小
         */
        void chunkInit(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/images/upload/chunk 上传单个分片
         * @return 通过 callback 返回 201 与分片状态
         */
        void uploadChunk(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/images/upload/merge 合并分片
         * @return 通过 callback 返回 201 与完整影像记录
         */
        void mergeChunks(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/images/upload-folder 上传整个影像目录
         * @return 通过 callback 返回 201 与影像记录; 主文件与附属文件一并上传
         */
        void uploadFolder(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/images/register-path 登记本地影像路径
         * @return 通过 callback 返回 201 与影像记录; 文件已在服务器上, 不复制
         */
        void registerPath(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/images/register-folder 批量登记本地目录影像
         * @return 通过 callback 返回 201 与登记结果
         */
        void registerFolder(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/images/register-oblique 登记倾斜摄影成果
         * @return 通过 callback 返回 201 与成果记录; 指向 tileset.json
         */
        void registerOblique(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/images/oblique-tileset?project_id= 查询倾斜摄影 tileset 地址
         * @return 通过 callback 返回项目可用的 tileset 地址
         */
        void getObliqueTileset(const drogon::HttpRequestPtr& req,
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/images/{id} 查询影像详情
         * @param id 影像 id
         * @return 通过 callback 返回影像记录
         */
        void getImage(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                      int id);

        /**
         * @brief DELETE /api/v1/images/{id} 删除影像
         * @param id 影像 id
         * @return 通过 callback 返回 204; 记录与文件一并删除
         */
        void deleteImage(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief GET /api/v1/images/{id}/download 下载影像文件
         * @param id 影像 id
         * @return 通过 callback 返回影像文件
         */
        void downloadImage(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief POST /api/v1/images/{id}/quality-check 执行影像质量检查
         * @param id 影像 id
         * @return 通过 callback 返回质检结果并回写状态
         */
        void qualityCheck(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief DELETE /api/v1/images/{id}/preprocess 删除预处理成果
         * @param id 影像 id
         * @return 通过 callback 返回 204
         */
        void deletePreprocess(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

    private:
        std::shared_ptr<services::ImageService> imageService_;
    };

}  // namespace soilwatercm::controllers
