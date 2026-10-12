#pragma once

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace soilwatercm::infrastructure::external_tools
{

    /// 子进程执行结果
    struct ProcessResult
    {
        int exit_code;
        std::string output;
        std::string error;
        bool timed_out;
    };

    /**
     * @brief 子进程管理
     *
     * 用 popen 启动外部命令行工具, 捕获 stdout/stderr, 支持超时。
     * 异步任务的取消/等待/状态查询当前均未实现。
     */
    class ProcessManager
    {
    public:
        ProcessManager();
        ~ProcessManager();

        /**
         * @brief 同步执行命令, 返回退出码和输出
         * @param command 可执行命令
         * @param args 参数列表 (含空格的自动加引号)
         * @param working_dir 工作目录, 空则用当前目录
         * @return 执行结果
         */
        ProcessResult execute(const std::string& command, const std::vector<std::string>& args,
                              const std::string& working_dir = "");

        /**
         * @brief 带超时的执行
         *
         * 超时后只打 timed_out 标记, 子进程仍在后台跑。
         * @param command 可执行命令
         * @param args 参数列表
         * @param timeout_seconds 超时秒数; <=0 表示不限时
         * @param working_dir 工作目录
         * @return 执行结果
         */
        ProcessResult executeWithTimeout(const std::string& command, const std::vector<std::string>& args,
                                         int timeout_seconds, const std::string& working_dir = "");

        /**
         * @brief 后台线程执行, 不等待结果
         * @param command 可执行命令
         * @param args 参数列表
         * @param working_dir 工作目录
         * @return 进程 id (仅用于日志, 后续无法查询)
         */
        std::string executeAsync(const std::string& command, const std::vector<std::string>& args,
                                 const std::string& working_dir = "");

        /// @brief 取消进程, 当前未实现, 只记日志恒返回 false
        bool cancel(const std::string& process_id);

        /// @brief 等待异步进程结果, 当前未实现, 恒返回空结果
        ProcessResult wait(const std::string& process_id, int timeout_seconds = -1);

        /// @brief 设置子进程统一使用的环境变量 (写入进程环境表并 setenv)
        void setEnvironment(const std::map<std::string, std::string>& env);

        /// @brief 查询进程是否运行, 当前未实现, 恒返回 false
        bool isRunning(const std::string& process_id);

        ProcessManager(const ProcessManager&) = delete;
        ProcessManager& operator=(const ProcessManager&) = delete;
        ProcessManager(ProcessManager&&) = delete;
        ProcessManager& operator=(ProcessManager&&) = delete;

    private:
        std::string buildCommand(const std::string& command, const std::vector<std::string>& args);

        std::map<std::string, std::string> environment_;
        std::mutex mutex_;
        std::atomic<bool> shutdown_{false};
    };

}  // namespace soilwatercm::infrastructure::external_tools
