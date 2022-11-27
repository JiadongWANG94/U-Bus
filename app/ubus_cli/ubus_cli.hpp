/**
 * Wang Jiadong <jiadong.wang.94@outlook.com>
 */

#pragma once

#include <iostream>
#include <string>
#include <thread>

#include "frame.hpp"
#include "ubus_runtime.hpp"

class StringMsg : public MessageBase {
 public:
    static const uint32_t id = 10000;

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

    bool echo_event(const std::string &topic);
};