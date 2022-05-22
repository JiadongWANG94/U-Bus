#pragma once

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <memory>
#include <thread>
#include <functional>
#include <unordered_map>

#include "nlohmann/json.hpp"

#include "log.hpp"

class Publisher {
 public:
    template <typename EventT>
    bool publish(const EventT &data);
};

class SerilizableDatatype {
 public:
    virtual ~SerilizableDatatype() {}
    virtual void serilize(std::string *data) = 0;
    virtual void unserilize(const std::string &data) = 0;
};

class EventType : public SerilizableDatatype {};

template <typename RequestT, typename ResponseT>
class MethodType {
 public:
    RequestT request;
    ResponseT response;
};

class UBusRuntime {
 public:
    bool init(const std::string &name, const std::string &ip, uint32_t port);

    template <typename EventT>
    bool subscribe(const std::string &topic,
                   std::function<void(const EventT &)> callback);

    template <typename EventT>
    bool register_publisher(const std::string &topic, Publisher *publisher);

    template <typename MethodT>
    bool call_method(const std::string &method, MethodT *data);

 private:
    int32_t control_sock_ = 0;
    std::shared_ptr<std::thread> keep_alive_worker_;

 private:
    void keep_alive_sender();
};
