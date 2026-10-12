#pragma once

#include <future>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../dao/image_dao.h"
#include "../infrastructure/file_manager.h"
#include "../infrastructure/gdal/gdal_wrapper.h"
#include "../models/image.h"

namespace soilwatercm::services
{

    /** @brief 单文件上传入参 */
    struct ImageUploadParams
    {
        std::string file_path;                   ///< 源文件路径 (须已落盘)
        std::string original_filename;           ///< 原始文件名
        int project_id;                          ///< 归属项目 id
        int period_id;                           ///< 归属期次 id
        models::ImageType type = models::ImageType::Optical;  ///< 影像类型
        std::optional<std::string> sensor_type;  ///< 传感器类型 (空则 Unknown)
    };

    /** @brief 表单上传/登记类接口的公共参数 (对齐 Python images.py 的 Form 字段) */
    struct ImageFormParams
    {
        int project_id = 0;                 ///< 归属项目 id
        int period_id = 0;                  ///< 归属期次 id
        std::string filename;               ///< 文件名, 可含相对路径 (保留文件夹结构)
        std::string source_type;            ///< 来源类型 (用于推断影像 type)
        std::string sensor;                 ///< 传感器
        std::string acquisition_date;       ///< 采集时间 (ISO 字符串)
        std::optional<double> resolution_m; ///< 分辨率 (米)
        std::string file_format;            ///< 文件格式 (空则按扩展名推断)
    };

    /** @brief 分块上传初始化结果 */
    struct ChunkInitResult
    {
        std::string upload_id;                    ///< 上传会话 id
        long long chunk_size = 50LL * 1024 * 1024; ///< 分片大小 (50MB)
    };

    /** @brief 影像质量检查结果 */
    struct ImageQualityCheckResult
    {
        bool is_valid = false;                    ///< 影像是否有效
        std::string message;                      ///< 检查说明
        double cloud_cover = 0.0;                 ///< 云量估计
        std::optional<models::Geometry> extent;   ///< 覆盖范围
        std::optional<std::string> crs;           ///< 坐标参考系
        std::optional<double> resolution;         ///< 分辨率
    };

    /**
     * @brief 影像数据管理服务
     *
     * 上传入库 (单文件/分块/批量/路径登记/倾斜 tileset)、删除、
     * 质量检查 (GDAL 读元数据判定有效性)。入库时生成影像编码并拷贝到存储目录。
     */
    class ImageService
    {
    public:
        /**
         * @brief 构造, 注入影像 DAO、文件管理与 GDAL 封装
         * @param imageDao 影像 DAO
         * @param fileManager 文件管理器 (存储目录)
         * @param gdal GDAL 封装 (质量检查读元数据)
         */
        explicit ImageService(std::shared_ptr<dao::ImageDao> imageDao,
                              std::shared_ptr<infrastructure::FileManager> fileManager,
                              std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal);

        ~ImageService() = default;

        /**
         * @brief 上传影像
         *
         * 拷贝到项目存储目录, 生成编码并入库 (质量状态置待检)。
         * @param params 上传入参
         * @return 含新 id 的影像记录
         * @throw FileNotFoundException 源文件不存在时
         */
        models::Image uploadImage(const ImageUploadParams& params);

        // ---- 分块上传 ----

        /**
         * @brief 初始化分块上传会话
         * @param projectId 项目 id
         * @param filename 原始文件名 (清洗后参与 upload_id)
         * @param totalSize 文件总大小 (当前未使用)
         * @return upload_id 与 50MB 分片大小; 同时建好分片临时目录
         */
        ChunkInitResult initChunkUpload(int projectId, const std::string& filename, long long totalSize);

        /**
         * @brief 写入单个分片到临时目录
         * @param projectId 项目 id
         * @param uploadId 上传会话 id
         * @param chunkIndex 分片序号 (从 0 起, 按序合并)
         * @param content 分片内容
         * @throw FileException 分片写入失败时
         */
        void saveChunk(int projectId, const std::string& uploadId, int chunkIndex, const std::string& content);

        /**
         * @brief 按序合并分片并入库, 随后清理分片临时目录
         * @param uploadId 上传会话 id
         * @param params 表单参数 (filename 校验防目录穿越)
         * @return 含新 id 的影像记录
         * @throw ValidationException 分片数据缺失 / filename 为绝对路径 / 非法文件名时
         */
        models::Image mergeChunks(const std::string& uploadId, const ImageFormParams& params);

        // ---- 批量上传 (multipart 主文件已落盘临时目录, 拷贝入库) ----

        /**
         * @brief 将 multipart 已落盘的临时文件拷贝入库
         * @param params 表单参数 (filename 校验防目录穿越)
         * @param tempPath 临时文件路径
         * @return 含新 id 的影像记录
         * @throw FileNotFoundException 临时文件不存在时
         * @throw ValidationException filename 非法时
         */
        models::Image importUploadedFile(const ImageFormParams& params, const std::string& tempPath);

        // ---- 路径登记 (不拷贝文件, 仅记录外部路径) ----

        /**
         * @brief 登记外部影像文件路径
         * @param params 表单参数
         * @param absolutePath 影像绝对路径
         * @return 含新 id 的影像记录
         * @throw ValidationException 路径非绝对或文件不存在时
         */
        models::Image registerExistingPath(const ImageFormParams& params, const std::string& absolutePath);

        /**
         * @brief 递归登记目录下影像文件 (tif/tiff/img/jp2/png/bmp)
         * @param params 表单参数
         * @param folderPath 目录绝对路径
         * @return (新影像 id 列表, 跳过原因列表)
         * @throw ValidationException 路径非绝对或目录不存在时
         */
        std::pair<std::vector<int>, std::vector<std::string>> registerFolder(const ImageFormParams& params,
                                                                             const std::string& folderPath);

        /**
         * @brief 登记倾斜摄影成果
         *
         * 传入目录或 tileset.json 路径, 递归找到 tileset.json 后按 3DTiles 入库。
         * @param params 表单参数
         * @param path 目录或 tileset.json 绝对路径
         * @return 含新 id 的影像记录
         * @throw ValidationException 路径非绝对或未找到 tileset.json 时
         */
        models::Image registerOblique(const ImageFormParams& params, const std::string& path);

        /**
         * @brief 删除预处理成果 (同目录 _rad/_geom/_ortho/_resamp 派生文件)
         * @param imageId 影像主键
         * @return 有派生文件被删返回 true; 无成果返回 false
         * @throw NotFoundException 影像不存在时
         */
        bool deletePreprocessResults(int imageId);

        /**
         * @brief 按 id 查影像
         * @param id 影像主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::Image> getImageById(int id);

        /**
         * @brief 按项目查影像列表
         * @param projectId 项目 id
         * @return 影像列表
         */
        std::vector<models::Image> getImagesByProject(int projectId);

        /**
         * @brief 按期次查影像列表
         * @param periodId 期次 id
         * @return 影像列表
         */
        std::vector<models::Image> getImagesByPeriod(int periodId);

        /**
         * @brief 删除影像
         * @param id 影像主键
         * @return 删除成功返回 true; 物理文件尽力删除
         * @throw NotFoundException 影像不存在时
         */
        bool deleteImage(int id);

        /**
         * @brief 取影像文件路径
         * @param id 影像主键
         * @return 文件路径
         * @throw NotFoundException 影像不存在时
         */
        std::string getImageFilePath(int id);

        /**
         * @brief 异步触发质量检查
         * @param imageId 影像主键
         * @return 检查结果 future
         */
        std::future<ImageQualityCheckResult> triggerQualityCheck(int imageId);

        /**
         * @brief 同步质量检查: GDAL 读栅格元数据, 结果回写质量状态
         * @param imageId 影像主键
         * @return 检查结果; 影像或文件不存在时返回 is_valid=false 并回写 Invalid
         */
        ImageQualityCheckResult performQualityCheck(int imageId);

        /**
         * @brief 更新质量状态与说明
         * @param imageId 影像主键
         * @param status 质量状态
         * @param message 说明
         * @return 更新成功返回 true
         */
        bool updateQualityStatus(int imageId, models::QualityStatus status, const std::string& message);

        /**
         * @brief 按质量状态查影像
         * @param status 质量状态
         * @return 影像列表
         */
        std::vector<models::Image> findImagesByQualityStatus(models::QualityStatus status);

    private:
        std::shared_ptr<dao::ImageDao> imageDao_;
        std::shared_ptr<infrastructure::FileManager> fileManager_;
        std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal_;

        std::string generateImageCode(const std::string& originalFilename);
        std::string generateStoragePath(int projectId, int periodId, const std::string& filename);
        ImageQualityCheckResult checkImageQuality(const std::string& filePath);

        // 表单类接口公共入库逻辑: 按参数创建影像记录 (type 由 source_type 推断, 时间解析失败取当前)
        // 入库失败抛 DatabaseException
        models::Image createImageRecord(const ImageFormParams& params, const std::string& filePath, size_t fileSize,
                                        const std::string& displayName);
        // 分片临时目录: {raw_dir}/uploads/{project_id}/{upload_id}
        std::string chunkDirPath(int projectId, const std::string& uploadId) const;
    };

}  // namespace soilwatercm::services
