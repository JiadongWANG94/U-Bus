/**
 * Wang Jiadong <jiadong.wang.94@outlook.com>
 */

#pragma once

#include <string>
#include <stdint.h>

#include "message.hpp"

#include "log.hpp"

#include "nlohmann/json.hpp"

class TestMessage1 : public MessageBase {
 public:
    static const uint32_t id;

 public:
    virtual void serialize(std::string *data) const override { *data = this->data; }
    virtual void deserialize(const std::string &data) override {
        LDEBUG(TestMessage1) << "got data " << data;
        this->data = data;
    }

    std::string data;
};

const uint32_t TestMessage1::id = 11;

class TestMessage2 : public MessageBase {
 public:
    static const uint32_t id;

 public:
    virtual void serialize(std::string *data) const override { *data = "Message2 Body"; }
    virtual void deserialize(const std::string &data) override { LDEBUG(TestMessage2) << "got data " << data; }
};

const uint32_t TestMessage2::id = 12;

class TimestampMessage : public MessageBase {
 public:
    static const uint32_t id;

 public:
    virtual void serialize(std::string *data) const override {
        nlohmann::json j;
        j["h"] = h;
        j["m"] = m;
        j["s"] = s;
        j["ms"] = ms;
        *data = j.dump();
    }
    virtual void deserialize(const std::string &data) override {
        try {
            nlohmann::json data_json = nlohmann::json::parse(data);
            h = data_json.at("h").get<uint16_t>();
            m = data_json.at("m").get<uint16_t>();
            s = data_json.at("s").get<uint16_t>();
            ms = data_json.at("ms").get<uint16_t>();
        } catch (nlohmann::json::exception &e) {
            LERROR(TimestampMessage) << "Exception in json : " << e.what();
        }
    }

 public:
    uint16_t h = 0;
    uint16_t m = 0;
    uint16_t s = 0;
    uint16_t ms = 0;
};

const uint32_t TimestampMessage::id = 13;