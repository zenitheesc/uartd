#pragma once
#include <map>
#include <nlohmann/json.hpp>
#include <string>

using message_t = std::vector<std::vector<char>>;

class FunctionHandler {
    static std::map<std::string, message_t (*)(const nlohmann::json&)> m_functionMap;

public:
    FunctionHandler() = default;

    template <typename T>
    FunctionHandler(T lambda, const std::string& name)
    {
        m_functionMap[name] = lambda;
    }

    [[nodiscard]] static auto getFunctionMap() -> std::map<std::string, message_t (*)(const nlohmann::json&)>
    {
        return m_functionMap;
    }
};

std::map<std::string, message_t (*)(const nlohmann::json&)> FunctionHandler::m_functionMap;

#define REGISTER_FUNCTION(name, function) \
    FunctionHandler name(function, #name);
