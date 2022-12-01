/**
 * Wang Jiadong <jiadong.wang.94@outlook.com>
 */

#pragma once

#include <iostream>
#include <string>
#include <thread>

#include "frame.hpp"
#include "ubus_runtime.hpp"

class DebugMsg : public MessageBase {
 public:
    uint32_t id;

 public:
    virtual void serialize(std::string *data) const { *data = this->data; }
    virtual void deserialize(const std::string &data) { this->data = data; }

 public:
    std::string data;
};

class UBusDebugger : public UBusRuntime {
 public:
    bool query_debug_info(const std::string &input, std::string *output);

    bool query_event_list(std::string *out = nullptr);

    bool query_method_list(std::string *out = nullptr);

    bool query_participant_list(std::string *out = nullptr);

    bool request_method(const std::string &method_name,
                        uint32_t request_type,
                        const std::string &request,
                        uint32_t response_type,
                        std::string *response);

    bool echo_event(const std::string &topic);
};