#pragma once

#include <stdexcept>
#include <string>

namespace soilwatercm::common
{

    /** @brief 业务异常基类, getType 返回类型名, 便于统一映射错误响应 */
    class SoilWaterCMException : public std::runtime_error
    {
    public:
        explicit SoilWaterCMException(const std::string& message) : std::runtime_error(message) {}

        virtual ~SoilWaterCMException() = default;

        /// @brief 异常类型名
        virtual std::string getType() const { return "SoilWaterCMException"; }
    };

    /** @brief 入参校验不通过异常, 用于 Controller/Service 层字段检查失败 */
    class ValidationException : public SoilWaterCMException
    {
    public:
        explicit ValidationException(const std::string& message) : SoilWaterCMException(message) {}

        std::string getType() const override { return "ValidationException"; }
    };

    /** @brief 数据库读写失败异常, 用于 SQL 执行或连接异常 */
    class DatabaseException : public SoilWaterCMException
    {
    public:
        explicit DatabaseException(const std::string& message) : SoilWaterCMException(message) {}

        std::string getType() const override { return "DatabaseException"; }
    };

    /** @brief 遥感处理任务执行出错异常, 用于 InSAR/影像处理流程失败 */
    class ProcessingException : public SoilWaterCMException
    {
    public:
        explicit ProcessingException(const std::string& message) : SoilWaterCMException(message) {}

        std::string getType() const override { return "ProcessingException"; }
    };

    /** @brief 认证授权异常, 用于登录、令牌、权限校验失败 */
    class AuthException : public SoilWaterCMException
    {
    public:
        explicit AuthException(const std::string& message) : SoilWaterCMException(message) {}

        std::string getType() const override { return "AuthException"; }
    };

    /** @brief 资源不存在异常, 用于按 id 查不到记录 */
    class NotFoundException : public SoilWaterCMException
    {
    public:
        explicit NotFoundException(const std::string& message) : SoilWaterCMException(message) {}

        std::string getType() const override { return "NotFoundException"; }
    };

    /** @brief 文件读写操作失败异常 */
    class FileException : public SoilWaterCMException
    {
    public:
        explicit FileException(const std::string& message) : SoilWaterCMException(message) {}

        std::string getType() const override { return "FileException"; }
    };

    /** @brief 目标文件不存在异常 */
    class FileNotFoundException : public FileException
    {
    public:
        explicit FileNotFoundException(const std::string& message) : FileException(message) {}

        std::string getType() const override { return "FileNotFoundException"; }
    };

    /** @brief GDAL/OGR 调用失败异常 */
    class GDALException : public SoilWaterCMException
    {
    public:
        explicit GDALException(const std::string& message) : SoilWaterCMException(message) {}

        std::string getType() const override { return "GDALException"; }
    };

    /** @brief 外部 GIS 工具进程执行失败异常 */
    class ExternalToolException : public SoilWaterCMException
    {
    public:
        explicit ExternalToolException(const std::string& message) : SoilWaterCMException(message) {}

        std::string getType() const override { return "ExternalToolException"; }
    };

}  // namespace soilwatercm::common
