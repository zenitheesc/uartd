#pragma once
#include "uartd/function-handler.hpp"
#include <cstring>
#include <iostream>
#include <vector>

REGISTER_FUNCTION(request, [](const nlohmann::json& json) {
    std::vector<uint8_t> message = json["data"];

    message_t res = {};

    size_t size = message.size() / 8 + static_cast<bool>(message.size() % 8);
    res.resize(size);

    for (size_t i = 0; i < message.size(); i += 8) {
        res[i / 8].resize(std::min(static_cast<size_t>(8), message.size() - i));
        std::memcpy(res[i / 8].data(), message.data() + i, std::min(static_cast<size_t>(8), message.size() - i));
    }

    return res;
})
