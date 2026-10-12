#pragma once

#include <memory>

#include <spdlog/spdlog.h>

#include "external_tool_interface.h"
#include "process_manager.h"

using namespace soilwatercm::infrastructure::external_tools;

namespace soilwatercm::infrastructure
{

    /**
     * @brief ESA SNAP 工具封装
     *
     * 把算法写成 Graph XML, 再调用 gpt 命令行执行。
     */
    class SnapInterface : public ExternalToolInterface
    {
    public:
        /**
         * @brief 构造, 探测 SNAP 安装位置
         * @param snapPath SNAP 安装目录, 空则自动探测
         */
        explicit SnapInterface(const std::string& snapPath = "");

        ~SnapInterface() override = default;

        /**
         * @brief 生成 Graph XML 后用 gpt 执行算法
         * @param algorithm 算法 id
         * @param params 参数键值表
         * @param callback 进度回调, 可空
         * @return 执行结果
         */
        ToolResult execute(const std::string& algorithm, const std::map<std::string, std::string>& params,
                           const ProgressCallback& callback = nullptr) override;

        /// @brief 异步执行, 后台线程调 execute
        std::future<ToolResult> executeAsync(const std::string& algorithm,
                                             const std::map<std::string, std::string>& params,
                                             const ProgressCallback& callback = nullptr) override;

        // 以下便捷方法都是 execute 的特定算法包装

        /// @brief 完整 InSAR 流程 (配准+干涉)
        ToolResult performInSAR(const std::string& masterProduct, const std::string& slaveProduct,
                                const std::string& outputDir);

        /// @brief 主辅影像配准
        ToolResult coregisterImages(const std::string& masterProduct, const std::string& slaveProduct,
                                    const std::string& outputDir);

        /// @brief 由配准结果生成干涉图
        ToolResult generateInterferogram(const std::string& coregisteredProduct, const std::string& outputDir);

        /// @brief 去除 Sentinel-1 条带边界并拼接
        ToolResult deburstAndMerge(const std::string& productPath, const std::string& output);

        /// @brief 用精密轨道文件替换元数据中的轨道信息
        ToolResult applyOrbitFile(const std::string& productPath, const std::string& output);

        /// @brief 地形相位去除 (地形平坦化)
        ToolResult terrainFlattening(const std::string& productPath, const std::string& demPath,
                                     const std::string& output);

        std::vector<AlgorithmInfo> listAlgorithms() override;

        bool isAvailable() override;

        std::string getVersion() override;

        std::string getName() override { return "SNAP"; }

    private:
        std::string snap_path_;
        std::string gpt_path_;
        std::unique_ptr<ProcessManager> process_manager_;
        std::shared_ptr<spdlog::logger> logger_;
        bool available_ = false;
        std::string version_;

        std::string buildGptCommand(const std::string& graphPath, const std::map<std::string, std::string>& params);

        std::string buildSnapCommand(const std::string& algorithm, const std::map<std::string, std::string>& params);

        bool detectSnapInstallation();

        std::string findGptPath();

        bool writeGraphXml(const std::string& graphPath, const std::string& algorithm,
                           const std::map<std::string, std::string>& params);
    };

}  // namespace soilwatercm::infrastructure
