#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace soilwatercm::infrastructure::external
{

    /// ODM 重建任务状态
    struct OdmTask
    {
        std::string id;
        std::string status;
        double progress;
        std::string output_path;
        std::string error_message;
    };

    /**
     * @brief OpenDroneMap 客户端
     *
     * 后台跑 odm 命令行做无人机影像重建, 查询任务状态、取正射/DSM/DTM 成果。
     */
    class OdmClient
    {
    public:
        /// @brief 构造, odm 命令默认为 "odm" (走 PATH)
        OdmClient();

        /**
         * @brief 构造, 指定 odm 可执行文件路径
         * @param odm_path odm 命令或可执行文件路径
         */
        explicit OdmClient(const std::string& odm_path);
        ~OdmClient() = default;

        /// @brief 设置 odm 可执行文件路径
        void setOdmPath(const std::string& path);

        /**
         * @brief 提交重建任务
         *
         * 由 ProcessManager 异步执行 odm 命令, 任务 id 记入活动进程表。
         * @param image_paths 影像路径列表
         * @param output_dir 输出目录
         * @return 任务 id
         */
        std::string submitTask(const std::vector<std::string>& image_paths, const std::string& output_dir);

        /**
         * @brief 查询任务状态
         * @param task_id 任务 id
         * @return 任务状态; 进程不在活动表中返回 not_found, 退出码非 0 返回 failed
         */
        OdmTask getTaskStatus(const std::string& task_id);

        /**
         * @brief 取消任务并终止其进程
         * @param task_id 任务 id
         * @return 取消成功返回 true; 任务不存在返回 false
         */
        bool cancelTask(const std::string& task_id);

        /**
         * @brief 获取重建成果文件路径
         * @param task_id 任务 id
         * @param result_type 成果类型: orthophoto / dsm / dtm
         * @return 成果文件路径; 不存在返回空串
         */
        std::string getResult(const std::string& task_id, const std::string& result_type);

        /// @brief 健康检查: 执行 odm --help 看退出码
        bool isHealthy();

        /// @brief 可用性检查: 路径存在或 odm --version 可执行
        bool isAvailable();

        /**
         * @brief 转换为三维瓦片
         *
         * 桩实现: 只生成占位 tileset.json, 并非真实三维瓦片转换。
         * @param inputPath 输入模型路径
         * @param outputPath 输出目录
         * @return 写入成功返回 true
         */
        bool convertTo3DTiles(const std::string& inputPath, const std::string& outputPath);

        OdmClient(const OdmClient&) = delete;
        OdmClient& operator=(const OdmClient&) = delete;
        OdmClient(OdmClient&&) = default;
        OdmClient& operator=(OdmClient&&) = default;

    private:
        std::string odm_path_;
        std::map<std::string, std::string> active_processes_;
    };

}  // namespace soilwatercm::infrastructure::external
