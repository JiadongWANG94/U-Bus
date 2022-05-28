/**
 * Wang Jiadong <jiadong.wang.94@outlook.com>
 */

#pragma once

#include <string>
#include <stdint.h>

#include "log.hpp"

class SerilizableDatatype {
 public:
    virtual ~SerilizableDatatype() {}
    virtual void serialize(std::string *data) const = 0;
    virtual void unserialize(const std::string &data) = 0;
};

class TestEvent1 : public SerilizableDatatype {
 public:
    static uint32_t id;

 public:
    virtual void serialize(std::string *data) const override { *data = this->data; }
    virtual void unserialize(const std::string &data) override {
        LDEBUG(TestEvent1) << "got data " << data << std::endl;
        this->data = data;
    }

    std::string data;
};

class TestEvent2 : public SerilizableDatatype {
 public:
    static uint32_t id;

 public:
    virtual void serialize(std::string *data) const override { *data = "Event2 Body"; }
    virtual void unserialize(const std::string &data) override {
        LDEBUG(TestEvent2) << "got data " << data << std::endl;
    }
};
