#pragma once

#include <memory>

#include <spdlog/spdlog.h>

#include "external_tool_interface.h"
#include "process_manager.h"

using namespace soilwatercm::infrastructure::external_tools;

namespace soilwatercm::infrastructure
{

    /**
     * @brief PyAPS3 封装
     *
     * 在 conda 环境里跑 Python 脚本, 用 ERA 等气象模型做大气 (对流层) 延迟改正。
     */
    class Pyaps3Interface : public ExternalToolInterface
    {
    public:
        /**
         * @brief 构造, 探测 conda 环境里 pyaps3 是否可用
         * @param condaEnv conda 环境名, 默认 mintpy
         */
        explicit Pyaps3Interface(const std::string& condaEnv = "mintpy");

        ~Pyaps3Interface() override = default;

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
         * @brief 下载指定时间范围的气象模型数据
         * @param dateStart 起始日期
         * @param dateEnd 结束日期
         * @param model 气象模型名 (如 ERA5)
         * @param outputDir 输出目录
         * @return 执行结果
         */
        ToolResult downloadAtmosphericData(const std::string& dateStart, const std::string& dateEnd,
                                           const std::string& model, const std::string& outputDir);

        /**
         * @brief 结合几何文件计算对流层延迟
         * @param atmosDataDir 气象数据目录
         * @param geometryFile 几何文件 (MintPy 的 geometry.rsc)
         * @param output 输出延迟图
         * @return 执行结果
         */
        ToolResult calculateTroposphericDelay(const std::string& atmosDataDir, const std::string& geometryFile,
                                              const std::string& output);

        /**
         * @brief 用对流层延迟图改正干涉图相位
         * @param ifgramFile 干涉图文件
         * @param tropoFile 对流层延迟图
         * @param output 改正后输出
         * @return 执行结果
         */
        ToolResult correctInterferogram(const std::string& ifgramFile, const std::string& tropoFile,
                                        const std::string& output);

        /// @brief 一步完成大气改正: 估计延迟并直接应用到干涉图
        bool correctAtmosphere(const std::string& inputPath, const std::string& outputPath);

        std::vector<AlgorithmInfo> listAlgorithms() override;

        bool isAvailable() override;

        std::string getVersion() override;

        std::string getName() override { return "pyaps3"; }

    private:
        std::string conda_env_;
        std::unique_ptr<ProcessManager> process_manager_;
        std::shared_ptr<spdlog::logger> logger_;
        bool available_ = false;
        std::string version_;

        std::string buildCondaCommand(const std::string& script);

        std::string buildPyaps3Command(const std::string& algorithm, const std::map<std::string, std::string>& params);

        bool checkPyaps3Available();
    };

}  // namespace soilwatercm::infrastructure
