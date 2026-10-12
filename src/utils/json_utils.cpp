#include "json_utils.h"

#include <fstream>
#include <sstream>

namespace soilwatercm::utils
{

    Json JsonUtils::parse(const std::string& str)
    {
        return Json::parse(str);
    }

    std::string JsonUtils::stringify(const Json& json, bool pretty)
    {
        if (pretty)
        {
            return json.dump(4);
        }
        return json.dump();
    }

    bool JsonUtils::has(const Json& json, const std::string& key)
    {
        return json.contains(key);
    }

    Json JsonUtils::merge(const Json& a, const Json& b)
    {
        Json result = a;

        if (!b.is_object())
        {
            return result;
        }

        for (auto it = b.begin(); it != b.end(); ++it)
        {
            result[it.key()] = it.value();
        }

        return result;
    }

    Json JsonUtils::fromFile(const std::string& file_path)
    {
        std::ifstream file(file_path);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open file: " + file_path);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        return parse(buffer.str());
    }

    bool JsonUtils::toFile(const std::string& file_path, const Json& json, bool pretty)
    {
        std::ofstream file(file_path);
        if (!file.is_open())
        {
            return false;
        }

        file << stringify(json, pretty);
        return true;
    }

    std::string JsonUtils::getErrorMessage(const std::exception& e)
    {
        return std::string(e.what());
    }

}  // namespace soilwatercm::utils
