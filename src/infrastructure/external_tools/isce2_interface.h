#pragma once

#include <memory>

#include <spdlog/spdlog.h>

#include "external_tool_interface.h"
#include "process_manager.h"

using namespace soilwatercm::infrastructure::external_tools;

namespace soilwatercm::infrastructure
{

    /**
     * @brief ISCE2 封装
     *
     * 在 conda 环境里跑 topsApp / stripmapApp 等处理链生成干涉图。
     */
    class Isce2Interface : public ExternalToolInterface
    {
    public:
        /**
         * @brief 构造, 探测 conda 环境里 ISCE2 是否可用
         * @param condaEnv conda 环境名, 默认 isce2
         */
        explicit Isce2Interface(const std::string& condaEnv = "isce2");

        ~Isce2Interface() override = default;

        /**
         * @brief 拼 conda run 命令执行算法; 环境不可用直接返回失败
         * @param algorithm 算法 id
         * @param params 参数键值表
         * @param callback 进度回调, 可空
         * @return 执行结果, outputs["output"] 为输出路径
         */
        ToolResult execute(const std::string& algorithm, const std::map<std::string, std::string>& params,
                           const ProgressCallback& callback = nullptr) override;

        /// @brief 异步执行, 后台线程调 execute
        std::future<ToolResult> executeAsync(const std::string& algorithm,
                                             const std::map<std::string, std::string>& params,
                                             const ProgressCallback& callback = nullptr) override;

        /**
         * @brief Sentinel-1 TOPS 模式完整处理
         *
         * 先写 topsApp.cfg 配置再执行, extraParams 覆盖默认配置项。
         * @param masterOrbit 主影像轨道文件
         * @param slaveOrbit 辅影像轨道文件
         * @param demPath DEM 路径
         * @param outputDir 输出目录
         * @param extraParams 额外配置项
         * @return 执行结果
         */
        ToolResult runTopsApp(const std::string& masterOrbit, const std::string& slaveOrbit, const std::string& demPath,
                              const std::string& outputDir, const std::map<std::string, std::string>& extraParams = {});

        /// @brief Stripmap 模式处理 (转发 execute)
        ToolResult runStripmapApp(const std::string& masterImage, const std::string& slaveImage,
                                  const std::string& demPath, const std::string& outputDir);

        /// @brief 主辅影像配准 (转发 execute)
        ToolResult coregisterImages(const std::string& masterImage, const std::string& slaveImage,
                                    const std::string& outputDir);

        /// @brief 生成干涉图 (转发 execute)
        ToolResult generateInterferogram(const std::string& masterImage, const std::string& slaveImage,
                                         const std::string& outputDir);

        std::vector<AlgorithmInfo> listAlgorithms() override;

        bool isAvailable() override;

        std::string getVersion() override;

        std::string getName() override { return "ISCE2"; }

    private:
        std::string conda_env_;
        std::unique_ptr<ProcessManager> process_manager_;
        std::shared_ptr<spdlog::logger> logger_;
        bool available_ = false;
        std::string version_;

        std::string buildCondaCommand(const std::string& script);

        std::string buildIsce2Command(const std::string& algorithm, const std::map<std::string, std::string>& params);

        bool checkIsce2Available();

        bool writeTopsAppConfig(const std::string& configPath, const std::map<std::string, std::string>& params);
    };

}  // namespace soilwatercm::infrastructure
