#pragma once

#include <memory>
#include <string>
#include <vector>

namespace soilwatercm::infrastructure::external
{

    /// InSAR 远程处理任务状态
    struct InsarTask
    {
        std::string id;
        std::string status;
        double progress;
        std::string output_path;
        std::string error_message;
    };

    /**
     * @brief 远程 InSAR 处理服务客户端
     *
     * 当前全部方法为桩实现: 提交任务只生成随机任务 id, 查询恒返回 completed,
     * 不真正执行处理。
     */
    class InsarClient
    {
    public:
        /// @brief 构造, 默认工具为 isce2
        InsarClient();
        ~InsarClient() = default;

        /**
         * @brief 提交差分干涉任务 (桩实现, 只返回随机任务 id)
         * @param master_image 主影像路径
         * @param slave_image 辅影像路径
         * @param output_dir 输出目录
         * @return 任务 id
         */
        std::string submitDinsarTask(const std::string& master_image, const std::string& slave_image,
                                     const std::string& output_dir);

        /**
         * @brief 提交时间序列任务 (桩实现, 只返回随机任务 id)
         * @param images 影像路径列表
         * @param output_dir 输出目录
         * @return 任务 id
         */
        std::string submitTimeseriesTask(const std::vector<std::string>& images, const std::string& output_dir);

        /**
         * @brief 查询任务状态 (桩实现, 恒返回 completed/100%)
         * @param task_id 任务 id
         * @return 任务状态
         */
        InsarTask getTaskStatus(const std::string& task_id);

        /// @brief 取消任务 (桩实现, 恒返回 true)
        bool cancelTask(const std::string& task_id);

        /// @brief 获取任务成果 (桩实现, 恒返回空串)
        std::string getResult(const std::string& task_id, const std::string& result_type);

        /// @brief 设置后端处理工具名
        void setTool(const std::string& tool_name);

        /// @brief 服务健康检查 (桩实现, 恒返回 true)
        bool isHealthy();

        InsarClient(const InsarClient&) = delete;
        InsarClient& operator=(const InsarClient&) = delete;
        InsarClient(InsarClient&&) = default;
        InsarClient& operator=(InsarClient&&) = default;

    private:
        std::string current_tool_;
    };

}  // namespace soilwatercm::infrastructure::external
