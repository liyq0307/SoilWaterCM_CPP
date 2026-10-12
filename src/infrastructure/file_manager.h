#pragma once

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace soilwatercm::infrastructure
{

    /**
     * @brief 文件操作封装
     *
     * 所有相对路径都基于构造时传入的根目录解析。
     */
    class FileManager
    {
    public:
        using Path = std::filesystem::path;

        /**
         * @brief 构造, 指定根目录
         * @param baseDir 相对路径的解析基准
         */
        explicit FileManager(const std::string& baseDir);
        ~FileManager() = default;

        /// @brief 把相对路径拼到根目录下得到绝对路径
        Path getAbsolutePath(const std::string& relativePath) const;

        /// @brief 路径是否存在
        bool exists(const std::string& path) const;

        /// @brief 是否目录
        bool isDirectory(const std::string& path) const;

        /// @brief 是否普通文件
        bool isFile(const std::string& path) const;

        /// @brief 创建单层目录
        bool createDirectory(const std::string& path) const;

        /// @brief 递归创建多级目录
        bool createDirectories(const std::string& path) const;

        /// @brief 删除空目录
        bool removeDirectory(const std::string& path) const;

        /// @brief 删除文件
        bool removeFile(const std::string& path) const;

        /// @brief 读整个文件为字符串; 读不到返回空串
        std::string readFile(const std::string& filePath) const;

        /// @brief 覆盖写入文件
        bool writeFile(const std::string& filePath, const std::string& content) const;

        /// @brief 追加写入文件
        bool appendFile(const std::string& filePath, const std::string& content) const;

        /// @brief 复制文件
        bool copyFile(const std::string& source, const std::string& destination) const;

        /// @brief 移动文件
        bool moveFile(const std::string& source, const std::string& destination) const;

        /// @brief 列目录下文件, recursive 时递归展开
        std::vector<std::string> listFiles(const std::string& directory, bool recursive = false) const;

        /// @brief 列目录下子目录, recursive 时递归展开
        std::vector<std::string> listDirectories(const std::string& directory, bool recursive = false) const;

        /// @brief 文件字节数; 不存在返回 0
        size_t getFileSize(const std::string& filePath) const;

        /// @brief 文件名 (含扩展名)
        std::string getFileName(const std::string& filePath) const;

        /// @brief 扩展名 (不含点)
        std::string getFileExtension(const std::string& filePath) const;

        /// @brief 父目录路径
        std::string getParentDirectory(const std::string& filePath) const;

        /// @brief 规范化路径 (消除 ./ 与 ../)
        std::string normalizePath(const std::string& path) const;

        /// @brief 拼接多级路径
        std::string joinPath(const std::vector<std::string>& parts) const;

        /// @brief 扩展名是否在给定列表中
        bool hasExtension(const std::string& filePath, const std::vector<std::string>& extensions) const;

        /// @brief 打开输入流
        std::ifstream openInputStream(const std::string& filePath) const;

        /// @brief 打开输出流, append 为 true 时追加
        std::ofstream openOutputStream(const std::string& filePath, bool append = false) const;

        FileManager(const FileManager&) = delete;
        FileManager& operator=(const FileManager&) = delete;
        FileManager(FileManager&&) = default;
        FileManager& operator=(FileManager&&) = default;

    private:
        Path base_dir_;
    };

}  // namespace soilwatercm::infrastructure
