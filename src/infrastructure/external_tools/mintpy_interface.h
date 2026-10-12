#pragma once

#include <memory>

#include <spdlog/spdlog.h>

#include "external_tool_interface.h"
#include "process_manager.h"

using namespace soilwatercm::infrastructure::external_tools;

namespace soilwatercm::infrastructure
{

    /**
     * @brief MintPy 封装
     *
     * 在 conda 环境里跑 smallbaselineApp / timeseries 等脚本, 做 SBAS 时序 InSAR 形变反演。
     */
    class MintPyInterface : public ExternalToolInterface
    {
    public:
        /**
         * @brief 构造, 探测 conda 环境里 MintPy 是否可用
         * @param condaEnv conda 环境名, 默认 mintpy
         */
        explicit MintPyInterface(const std::string& condaEnv = "mintpy");

        ~MintPyInterface() override = default;

        /**
         * @brief 拼 conda run 命令执行算法
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

        /**
         * @brief 跑小基线集处理
         * @param templateFile MintPy 模板配置文件
         * @param projectDir 工程目录
         * @return 执行结果
         */
        ToolResult runSmallBaselineApp(const std::string& templateFile, const std::string& projectDir);

        /// @brief 生成形变速率图
        ToolResult generateVelocityMap(const std::string& projectDir, const std::string& output);

        /// @brief 生成形变时序结果
        ToolResult generateTimeSeries(const std::string& projectDir, const std::string& output);

        /**
         * @brief 大气改正
         * @param projectDir 工程目录 (须含 timeseries.h5)
         * @param method 改正方法: ERA5 / GACOS 等
         * @return 执行结果
         */
        ToolResult correctAtmosphere(const std::string& projectDir, const std::string& method = "ERA5");

        /// @brief 估计 DEM 误差引起的形变分量
        ToolResult estimateDemError(const std::string& projectDir, const std::string& output);

        /// @brief 生成 SBAS 模板配置 (当前写默认模板, 不读工程参数)
        bool generateConfig(const std::string& projectDir, const std::string& configPath);

        /// @brief 按配置跑 smallbaselineApp, 配合 generateConfig 两步使用
        bool runSbas(const std::string& configPath, const std::string& outputDir);

        std::vector<AlgorithmInfo> listAlgorithms() override;

        bool isAvailable() override;

        std::string getVersion() override;

        std::string getName() override { return "MintPy"; }

    private:
        std::string conda_env_;
        std::unique_ptr<ProcessManager> process_manager_;
        std::shared_ptr<spdlog::logger> logger_;
        bool available_ = false;
        std::string version_;

        std::string buildCondaCommand(const std::string& script);

        std::string buildMintPyCommand(const std::string& algorithm, const std::map<std::string, std::string>& params);

        bool checkMintPyAvailable();

        bool writeSmallBaselineTemplate(const std::string& templatePath,
                                        const std::map<std::string, std::string>& params);
    };

}  // namespace soilwatercm::infrastructure
