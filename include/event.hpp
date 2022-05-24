#pragma once

#include <string>
#include <stdint.h>

#include "log.hpp"

class SerilizableDatatype {
 public:
    virtual ~SerilizableDatatype() {}
    virtual void serilize(std::string *data) const = 0;
    virtual void unserilize(const std::string &data) = 0;
};

class EventType : public SerilizableDatatype {};

class TestEvent1 : public EventType {
 public:
    static uint32_t id;

 public:
    virtual void serilize(std::string *data) const override {
        *data = this->data;
    }
    virtual void unserilize(const std::string &data) override {
        LDEBUG(TestEvent1) << "got data " << data << std::endl;
        this->data = data;
    }

    std::string data;
};

class TestEvent2 : public EventType {
 public:
    static uint32_t id;

 public:
    virtual void serilize(std::string *data) const override {
        *data = "Event2 Body";
    }
    virtual void unserilize(const std::string &data) override {
        LDEBUG(TestEvent2) << "got data " << data << std::endl;
    }
};

template <typename RequestT, typename ResponseT>
class MethodType {
 public:
    RequestT request;
    ResponseT response;
};
