#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace soilwatercm::utils
{

    using Json = nlohmann::json;

    /**
     * @brief nlohmann::json 的便捷包装
     *
     * 解析/序列化、安全取值、文件读写、对象合并。
     */
    class JsonUtils
    {
    public:
        /**
         * @brief 解析 JSON 文本
         * @param str JSON 字符串
         * @return 解析结果; 格式非法抛异常
         */
        static nlohmann::json parse(const std::string& str);

        /// @brief 序列化为字符串, pretty 时缩进 4 空格
        static std::string stringify(const nlohmann::json& json, bool pretty = false);

        // 取键值: 不存在或类型不符返回 nullopt / 默认值

        /**
         * @brief 按键取指定类型的值
         * @param json 源对象
         * @param key 键名
         * @return 值; 不存在或类型不符返回 nullopt
         */
        template <typename T>
        static std::optional<T> get(const nlohmann::json& json, const std::string& key);

        /// @brief 按键取值, 取不到返回 default_value
        template <typename T>
        static T getOrDefault(const nlohmann::json& json, const std::string& key, const T& default_value);

        /// @brief 键是否存在
        static bool has(const nlohmann::json& json, const std::string& key);

        /// @brief 顶层键浅合并, b 的同名键覆盖 a
        static nlohmann::json merge(const nlohmann::json& a, const nlohmann::json& b);

        /**
         * @brief 从文件读 JSON
         * @param file_path 文件路径
         * @return 解析结果; 打不开或格式非法抛异常
         */
        static nlohmann::json fromFile(const std::string& file_path);

        /// @brief 写 JSON 到文件
        static bool toFile(const std::string& file_path, const nlohmann::json& json, bool pretty = false);

        /// @brief 取异常信息文本
        static std::string getErrorMessage(const std::exception& e);
    };

    template <typename T>
    std::optional<T> JsonUtils::get(const nlohmann::json& json, const std::string& key)
    {
        if (json.contains(key))
        {
            try
            {
                return json[key].get<T>();
            }
            catch (const std::exception&)
            {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    template <typename T>
    T JsonUtils::getOrDefault(const nlohmann::json& json, const std::string& key, const T& default_value)
    {
        if (json.contains(key))
        {
            try
            {
                return json[key].get<T>();
            }
            catch (const std::exception&)
            {
                return default_value;
            }
        }
        return default_value;
    }

}  // namespace soilwatercm::utils
