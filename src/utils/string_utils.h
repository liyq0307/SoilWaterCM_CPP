#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace soilwatercm::utils
{

    /**
     * @brief 字符串静态工具
     *
     * 裁剪、大小写、分割拼接、替换填充、URL 编解码等。
     */
    class StringUtils
    {
    public:
        /// @brief 去除首尾空白
        static std::string trim(const std::string& str);
        static std::string trimLeft(const std::string& str);
        static std::string trimRight(const std::string& str);

        static std::string toUpper(const std::string& str);
        static std::string toLower(const std::string& str);

        /// @brief 按分隔符切分
        static std::vector<std::string> split(const std::string& str, char delimiter);
        static std::vector<std::string> split(const std::string& str, const std::string& delimiter);

        /// @brief 用分隔符拼接
        static std::string join(const std::vector<std::string>& parts, const std::string& delimiter);

        static bool startsWith(const std::string& str, const std::string& prefix);
        static bool endsWith(const std::string& str, const std::string& suffix);

        static bool contains(const std::string& str, const std::string& substr);

        /// @brief 替换首个匹配
        static std::string replace(const std::string& str, const std::string& from, const std::string& to);

        /// @brief 替换全部匹配
        static std::string replaceAll(const std::string& str, const std::string& from, const std::string& to);

        static std::string padLeft(const std::string& str, size_t length, char pad_char = ' ');
        static std::string padRight(const std::string& str, size_t length, char pad_char = ' ');

        /// @brief 超长截断, 末尾加 suffix
        static std::string truncate(const std::string& str, size_t max_length, const std::string& suffix = "...");

        /// @brief 空串
        static bool isEmpty(const std::string& str);

        /// @brief 空串或全为空白
        static bool isBlank(const std::string& str);

        /// @brief 字节数转可读大小 (B/KB/MB/GB)
        static std::string formatSize(size_t bytes);

        /// @brief 数值按精度格式化
        static std::string formatNumber(double number, int precision = 2);

        /// @brief URL 百分号编码
        static std::string urlEncode(const std::string& str);

        /// @brief URL 百分号解码
        static std::string urlDecode(const std::string& str);
    };

}  // namespace soilwatercm::utils
