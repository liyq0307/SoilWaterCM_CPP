#pragma once

#include <memory>

#include <spdlog/spdlog.h>

#include "external_tool_interface.h"
#include "process_manager.h"

using namespace soilwatercm::infrastructure::external_tools;

namespace soilwatercm::infrastructure
{

    /**
     * @brief OTB 调用方式
     */
    enum class OtbCallMethod
    {
        CommandLine,
        CppApi
    };

    /**
     * @brief Orfeo ToolBox 封装
     *
     * 默认走 otbcli 命令行; CppApi 方式当前未实现, 调用会返回失败。
     */
    class OtbInterface : public ExternalToolInterface
    {
    public:
        /**
         * @brief 构造, 探测 OTB 安装
         * @param otbRootPath OTB 安装根目录
         * @param method 调用方式, 默认命令行
         */
        explicit OtbInterface(const std::string& otbRootPath, OtbCallMethod method = OtbCallMethod::CommandLine);

        ~OtbInterface() override = default;

        /**
         * @brief 执行算法 (otbcli 拼命令执行)
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

        /// @brief 影像分类 (用训练模型)
        ToolResult classifyImage(const std::string& imagePath, const std::string& modelPath, const std::string& output);

        /// @brief 两期影像变化检测
        ToolResult detectChange(const std::string& beforeImage, const std::string& afterImage,
                                const std::string& output);

        /**
         * @brief 多尺度分割
         * @param imagePath 输入影像
         * @param output 输出分割图
         * @param spatialThreshold 空间尺度阈值, 控制分割粒度
         * @param spectralThreshold 光谱尺度阈值
         * @return 执行结果
         */
        ToolResult segmentImage(const std::string& imagePath, const std::string& output, double spatialThreshold = 5.0,
                                double spectralThreshold = 15.0);

        /// @brief 提取影像特征 (纹理/几何等)
        ToolResult extractFeatures(const std::string& imagePath, const std::string& output,
                                   const std::vector<std::string>& features);

        /// @brief 光学影像辐射定标
        ToolResult opticalCalibration(const std::string& inputPath, const std::string& outputPath);

        /// @brief 正射校正, demPath 为空则不做地形校正
        ToolResult orthoRectification(const std::string& inputPath, const std::string& outputPath,
                                      const std::string& demPath = "");

        /// @brief 变化检测 (同 detectChange)
        ToolResult changeDetection(const std::string& beforePath, const std::string& afterPath,
                                   const std::string& outputPath);

        std::vector<AlgorithmInfo> listAlgorithms() override;

        bool isAvailable() override;

        std::string getVersion() override;

        std::string getName() override { return "Orfeo ToolBox"; }

    private:
        std::string otb_root_path_;
        OtbCallMethod call_method_;
        std::unique_ptr<ProcessManager> process_manager_;
        std::shared_ptr<spdlog::logger> logger_;
        bool available_ = false;
        std::string version_;

        ToolResult executeCommandLine(const std::string& algorithm, const std::map<std::string, std::string>& params,
                                      const ProgressCallback& callback);

        std::string buildCommandLine(const std::string& algorithm, const std::map<std::string, std::string>& params);

        bool detectOtbInstallation();

        std::string findOtbcliPath();
    };

}  // namespace soilwatercm::infrastructure
