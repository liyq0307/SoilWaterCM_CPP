#include "string_utils.h"

#include <cctype>
#include <iomanip>
#include <sstream>

namespace soilwatercm::utils
{

    std::string StringUtils::trim(const std::string& str)
    {
        return trimLeft(trimRight(str));
    }

    std::string StringUtils::trimLeft(const std::string& str)
    {
        auto it = std::find_if(str.begin(), str.end(), [](char c) { return !std::isspace(c); });
        return std::string(it, str.end());
    }

    std::string StringUtils::trimRight(const std::string& str)
    {
        auto it = std::find_if(str.rbegin(), str.rend(), [](char c) { return !std::isspace(c); });
        return std::string(str.begin(), it.base());
    }

    std::string StringUtils::toUpper(const std::string& str)
    {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    }

    std::string StringUtils::toLower(const std::string& str)
    {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

    std::vector<std::string> StringUtils::split(const std::string& str, char delimiter)
    {
        std::vector<std::string> parts;
        std::istringstream iss(str);
        std::string part;

        while (std::getline(iss, part, delimiter))
        {
            parts.push_back(part);
        }

        return parts;
    }

    std::vector<std::string> StringUtils::split(const std::string& str, const std::string& delimiter)
    {
        std::vector<std::string> parts;

        if (delimiter.empty())
        {
            parts.push_back(str);
            return parts;
        }

        size_t start = 0;
        size_t end = str.find(delimiter);

        while (end != std::string::npos)
        {
            parts.push_back(str.substr(start, end - start));
            start = end + delimiter.length();
            end = str.find(delimiter, start);
        }

        parts.push_back(str.substr(start));
        return parts;
    }

    std::string StringUtils::join(const std::vector<std::string>& parts, const std::string& delimiter)
    {
        if (parts.empty())
        {
            return "";
        }

        std::ostringstream oss;
        oss << parts[0];

        for (size_t i = 1; i < parts.size(); ++i)
        {
            oss << delimiter << parts[i];
        }

        return oss.str();
    }

    bool StringUtils::startsWith(const std::string& str, const std::string& prefix)
    {
        if (prefix.size() > str.size())
        {
            return false;
        }
        return str.compare(0, prefix.size(), prefix) == 0;
    }

    bool StringUtils::endsWith(const std::string& str, const std::string& suffix)
    {
        if (suffix.size() > str.size())
        {
            return false;
        }
        return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    bool StringUtils::contains(const std::string& str, const std::string& substr)
    {
        return str.find(substr) != std::string::npos;
    }

    std::string StringUtils::replace(const std::string& str, const std::string& from, const std::string& to)
    {
        size_t pos = str.find(from);
        if (pos == std::string::npos)
        {
            return str;
        }

        std::string result = str;
        result.replace(pos, from.length(), to);
        return result;
    }

    std::string StringUtils::replaceAll(const std::string& str, const std::string& from, const std::string& to)
    {
        if (from.empty())
        {
            return str;
        }

        std::string result = str;
        size_t pos = 0;

        while ((pos = result.find(from, pos)) != std::string::npos)
        {
            result.replace(pos, from.length(), to);
            pos += to.length();
        }

        return result;
    }

    std::string StringUtils::padLeft(const std::string& str, size_t length, char pad_char)
    {
        if (str.length() >= length)
        {
            return str;
        }
        return std::string(length - str.length(), pad_char) + str;
    }

    std::string StringUtils::padRight(const std::string& str, size_t length, char pad_char)
    {
        if (str.length() >= length)
        {
            return str;
        }
        return str + std::string(length - str.length(), pad_char);
    }

    std::string StringUtils::truncate(const std::string& str, size_t max_length, const std::string& suffix)
    {
        if (str.length() <= max_length)
        {
            return str;
        }
        return str.substr(0, max_length - suffix.length()) + suffix;
    }

    bool StringUtils::isEmpty(const std::string& str)
    {
        return str.empty();
    }

    bool StringUtils::isBlank(const std::string& str)
    {
        return trim(str).empty();
    }

    std::string StringUtils::formatSize(size_t bytes)
    {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unit_index = 0;
        double size = static_cast<double>(bytes);

        while (size >= 1024.0 && unit_index < 4)
        {
            size /= 1024.0;
            ++unit_index;
        }

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
        return oss.str();
    }

    std::string StringUtils::formatNumber(double number, int precision)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(precision) << number;
        return oss.str();
    }

    std::string StringUtils::urlEncode(const std::string& str)
    {
        std::ostringstream oss;
        oss << std::hex;

        for (char c : str)
        {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            {
                oss << c;
            }
            else
            {
                oss << std::uppercase;
                oss << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(c));
                oss << std::nouppercase;
            }
        }

        return oss.str();
    }

    std::string StringUtils::urlDecode(const std::string& str)
    {
        std::ostringstream oss;

        for (size_t i = 0; i < str.length(); ++i)
        {
            if (str[i] == '%' && i + 2 < str.length())
            {
                std::string hex = str.substr(i + 1, 2);
                oss << static_cast<char>(std::stoi(hex, nullptr, 16));
                i += 2;
            }
            else if (str[i] == '+')
            {
                oss << ' ';
            }
            else
            {
                oss << str[i];
            }
        }

        return oss.str();
    }

}  // namespace soilwatercm::utils
