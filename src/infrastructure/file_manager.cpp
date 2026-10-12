#include "file_manager.h"

#include <algorithm>
#include <sstream>

namespace soilwatercm::infrastructure
{

    FileManager::FileManager(const std::string& baseDir) : base_dir_(std::filesystem::absolute(baseDir))
    {
        if (!std::filesystem::exists(base_dir_))
        {
            std::filesystem::create_directories(base_dir_);
        }
    }

    FileManager::Path FileManager::getAbsolutePath(const std::string& relativePath) const
    {
        return std::filesystem::absolute(base_dir_ / relativePath);
    }

    bool FileManager::exists(const std::string& path) const
    {
        Path full_path = getAbsolutePath(path);
        return std::filesystem::exists(full_path);
    }

    bool FileManager::isDirectory(const std::string& path) const
    {
        Path full_path = getAbsolutePath(path);
        return std::filesystem::exists(full_path) && std::filesystem::is_directory(full_path);
    }

    bool FileManager::isFile(const std::string& path) const
    {
        Path full_path = getAbsolutePath(path);
        return std::filesystem::exists(full_path) && std::filesystem::is_regular_file(full_path);
    }

    bool FileManager::createDirectory(const std::string& path) const
    {
        Path full_path = getAbsolutePath(path);
        return std::filesystem::create_directory(full_path);
    }

    bool FileManager::createDirectories(const std::string& path) const
    {
        Path full_path = getAbsolutePath(path);
        return std::filesystem::create_directories(full_path);
    }

    bool FileManager::removeDirectory(const std::string& path) const
    {
        Path full_path = getAbsolutePath(path);
        return std::filesystem::remove_all(full_path) > 0;
    }

    bool FileManager::removeFile(const std::string& path) const
    {
        Path full_path = getAbsolutePath(path);
        return std::filesystem::remove(full_path);
    }

    std::string FileManager::readFile(const std::string& filePath) const
    {
        Path full_path = getAbsolutePath(filePath);

        std::ifstream file(full_path, std::ios::binary);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open file: " + filePath);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    bool FileManager::writeFile(const std::string& filePath, const std::string& content) const
    {
        Path full_path = getAbsolutePath(filePath);

        Path parent = full_path.parent_path();
        if (!std::filesystem::exists(parent))
        {
            std::filesystem::create_directories(parent);
        }

        std::ofstream file(full_path, std::ios::binary);
        if (!file.is_open())
        {
            return false;
        }

        file << content;
        return true;
    }

    bool FileManager::appendFile(const std::string& filePath, const std::string& content) const
    {
        Path full_path = getAbsolutePath(filePath);

        std::ofstream file(full_path, std::ios::binary | std::ios::app);
        if (!file.is_open())
        {
            return false;
        }

        file << content;
        return true;
    }

    bool FileManager::copyFile(const std::string& source, const std::string& destination) const
    {
        Path src_path = getAbsolutePath(source);
        Path dest_path = getAbsolutePath(destination);

        if (!std::filesystem::exists(src_path))
        {
            return false;
        }

        Path dest_parent = dest_path.parent_path();
        if (!std::filesystem::exists(dest_parent))
        {
            std::filesystem::create_directories(dest_parent);
        }

        std::filesystem::copy_file(src_path, dest_path, std::filesystem::copy_options::overwrite_existing);
        return true;
    }

    bool FileManager::moveFile(const std::string& source, const std::string& destination) const
    {
        Path src_path = getAbsolutePath(source);
        Path dest_path = getAbsolutePath(destination);

        if (!std::filesystem::exists(src_path))
        {
            return false;
        }

        Path dest_parent = dest_path.parent_path();
        if (!std::filesystem::exists(dest_parent))
        {
            std::filesystem::create_directories(dest_parent);
        }

        std::filesystem::rename(src_path, dest_path);
        return true;
    }

    std::vector<std::string> FileManager::listFiles(const std::string& directory, bool recursive) const
    {
        std::vector<std::string> files;
        Path dir_path = getAbsolutePath(directory);

        if (!std::filesystem::exists(dir_path) || !std::filesystem::is_directory(dir_path))
        {
            return files;
        }

        if (recursive)
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dir_path))
            {
                if (entry.is_regular_file())
                {
                    files.push_back(std::filesystem::relative(entry.path(), base_dir_).string());
                }
            }
        }
        else
        {
            for (const auto& entry : std::filesystem::directory_iterator(dir_path))
            {
                if (entry.is_regular_file())
                {
                    files.push_back(std::filesystem::relative(entry.path(), base_dir_).string());
                }
            }
        }

        std::sort(files.begin(), files.end());
        return files;
    }

    std::vector<std::string> FileManager::listDirectories(const std::string& directory, bool recursive) const
    {
        std::vector<std::string> directories;
        Path dir_path = getAbsolutePath(directory);

        if (!std::filesystem::exists(dir_path) || !std::filesystem::is_directory(dir_path))
        {
            return directories;
        }

        if (recursive)
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dir_path))
            {
                if (entry.is_directory())
                {
                    directories.push_back(std::filesystem::relative(entry.path(), base_dir_).string());
                }
            }
        }
        else
        {
            for (const auto& entry : std::filesystem::directory_iterator(dir_path))
            {
                if (entry.is_directory())
                {
                    directories.push_back(std::filesystem::relative(entry.path(), base_dir_).string());
                }
            }
        }

        std::sort(directories.begin(), directories.end());
        return directories;
    }

    size_t FileManager::getFileSize(const std::string& filePath) const
    {
        Path full_path = getAbsolutePath(filePath);
        if (!std::filesystem::exists(full_path))
        {
            return 0;
        }
        return std::filesystem::file_size(full_path);
    }

    std::string FileManager::getFileName(const std::string& filePath) const
    {
        Path full_path(filePath);
        return full_path.filename().string();
    }

    std::string FileManager::getFileExtension(const std::string& filePath) const
    {
        Path full_path(filePath);
        return full_path.extension().string();
    }

    std::string FileManager::getParentDirectory(const std::string& filePath) const
    {
        Path full_path(filePath);
        return full_path.parent_path().string();
    }

    std::string FileManager::normalizePath(const std::string& path) const
    {
        Path normalized = Path(path).lexically_normal();
        return normalized.string();
    }

    std::string FileManager::joinPath(const std::vector<std::string>& parts) const
    {
        Path result;
        for (const auto& part : parts)
        {
            result /= part;
        }
        return result.string();
    }

    bool FileManager::hasExtension(const std::string& filePath, const std::vector<std::string>& extensions) const
    {
        std::string ext = getFileExtension(filePath);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        for (std::string valid_ext : extensions)
        {
            std::transform(valid_ext.begin(), valid_ext.end(), valid_ext.begin(), ::tolower);
            if (ext == valid_ext || ext == "." + valid_ext)
            {
                return true;
            }
        }
        return false;
    }

    std::ifstream FileManager::openInputStream(const std::string& filePath) const
    {
        Path full_path = getAbsolutePath(filePath);
        return std::ifstream(full_path, std::ios::binary);
    }

    std::ofstream FileManager::openOutputStream(const std::string& filePath, bool append) const
    {
        Path full_path = getAbsolutePath(filePath);

        Path parent = full_path.parent_path();
        if (!std::filesystem::exists(parent))
        {
            std::filesystem::create_directories(parent);
        }

        if (append)
        {
            return std::ofstream(full_path, std::ios::binary | std::ios::app);
        }
        else
        {
            return std::ofstream(full_path, std::ios::binary);
        }
    }

}  // namespace soilwatercm::infrastructure
