/**
 * Wang Jiadong <jiadong.wang.94@outlook.com>
 */

#pragma once

#include <string>
#include <stdlib.h>

#include "definitions.hpp"

class MessageBase {
 public:
    virtual ~MessageBase() {}
    virtual void serialize(std::string *data) const = 0;
    virtual void deserialize(const std::string &data) = 0;
};

class NullMsg : public MessageBase {
 public:
    static const uint32_t id;

 public:
    virtual void serialize(std::string *data) const {}
    virtual void deserialize(const std::string &data) {}
};

class Int32Msg : public MessageBase {
 public:
    static const uint32_t id;

 public:
    virtual void serialize(std::string *data) const { *data = std::to_string(this->data); }
    virtual void deserialize(const std::string &data) { this->data = atoi(data.c_str()); }

 public:
    int32_t data;
};

class Int64Msg : public MessageBase {
 public:
    static const uint32_t id;

 public:
    virtual void serialize(std::string *data) const { *data = std::to_string(this->data); }
    virtual void deserialize(const std::string &data) { this->data = atol(data.c_str()); }

 public:
    int64_t data;
};

class Float32Msg : public MessageBase {
 public:
    static const uint32_t id;

 public:
    virtual void serialize(std::string *data) const { *data = std::to_string(this->data); }
    virtual void deserialize(const std::string &data) { this->data = atof(data.c_str()); }

 public:
    float32_t data;
};

class Float64Msg : public MessageBase {
 public:
    static const uint32_t id;

 public:
    virtual void serialize(std::string *data) const { *data = std::to_string(this->data); }
    virtual void deserialize(const std::string &data) { this->data = atof(data.c_str()); }

 public:
    float64_t data;
};

class StringMsg : public MessageBase {
 public:
    static const uint32_t id;

 public:
    virtual void serialize(std::string *data) const { *data = this->data; }
    virtual void deserialize(const std::string &data) { this->data = data; }

 public:
    std::string data;
};