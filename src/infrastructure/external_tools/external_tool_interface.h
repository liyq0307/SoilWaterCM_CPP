#pragma once

#include <functional>
#include <future>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace soilwatercm::infrastructure::external_tools
{

    /// 工具安装信息 (名称/路径/版本/是否可用)
    struct ToolInfo
    {
        std::string name;
        std::string path;
        std::string version;
        bool available;
    };

    /// 一次工具调用的结果
    struct ToolResult
    {
        bool success = false;
        std::string output;
        std::string error;
        int exit_code = 0;
        std::map<std::string, std::string> outputs;
    };

    /// 工具支持的一个算法的描述
    struct AlgorithmInfo
    {
        std::string id;
        std::string name;
        std::string description;
        std::vector<std::string> parameters;
    };

    /// 进度回调: (进度百分比, 状态消息)
    using ProgressCallback = std::function<void(double progress, const std::string& message)>;

    /**
     * @brief 外部 GIS 工具的统一接口
     *
     * 各工具 (SNAP/SAGA/OTB 等) 实现它, 调用方按"算法名 + 参数键值"的方式使用,
     * 不关心底层是命令行还是脚本。
     */
    class ExternalToolInterface
    {
    public:
        virtual ~ExternalToolInterface() = default;

        /// @brief 工具在本机是否可用
        virtual bool isAvailable() = 0;

        /// @brief 工具名称
        virtual std::string getName() = 0;

        /// @brief 检测到的工具版本
        virtual std::string getVersion() = 0;

        /**
         * @brief 同步执行指定算法
         * @param algorithm 算法 id
         * @param params 参数键值表
         * @param callback 进度回调, 可空
         * @return 执行结果
         */
        virtual ToolResult execute(const std::string& algorithm, const std::map<std::string, std::string>& params,
                                   const ProgressCallback& callback = nullptr) = 0;

        /**
         * @brief 异步执行指定算法
         * @param algorithm 算法 id
         * @param params 参数键值表
         * @param callback 进度回调, 可空
         * @return 结果的 future
         */
        virtual std::future<ToolResult> executeAsync(const std::string& algorithm,
                                                     const std::map<std::string, std::string>& params,
                                                     const ProgressCallback& callback = nullptr) = 0;

        /// @brief 列出该工具支持的算法
        virtual std::vector<AlgorithmInfo> listAlgorithms() = 0;
    };

}  // namespace soilwatercm::infrastructure::external_tools
