/**
 * Wang Jiadong <jiadong.wang.94@outlook.com>
 */

#pragma once

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <list>
#include <atomic>
#include <string>
#include <memory>
#include <thread>
#include <queue>
#include <functional>
#include <unordered_map>

#include "nlohmann/json.hpp"

#include "event.hpp"
#include "log.hpp"
#include "frame.hpp"
#include "helpers.hpp"

class UBusRuntime {
 public:
    bool init(const std::string &name, const std::string &ip, uint32_t port);

    template <typename EventT>
    bool subscribe_event(const std::string &topic, std::function<void(const EventT &)> callback);

    template <typename EventT>
    bool advertise_event(const std::string &topic);

    template <typename EventT>
    bool publish_event(const std::string &topic, const EventT &event);

    template <typename RequestT, typename ResponseT>
    bool provide_method(const std::string &method, std::function<void(const RequestT &, ResponseT *)> callback);

    template <typename RequestT, typename ResponseT>
    bool call_method(const std::string &method, const RequestT &request, ResponseT *response);

 private:
    int32_t control_sock_ = 0;
    int32_t listening_sock_ = 0;
    std::shared_ptr<std::thread> listening_worker_;
    std::shared_ptr<std::thread> event_worker_;
    std::shared_ptr<std::thread> keep_alive_worker_;
    struct PubEventInfo {
        std::string topic;
        uint32_t type = 0;
        std::unordered_map<std::string, int32_t> client_socket_map;
    };
    std::unordered_map<std::string, PubEventInfo> pub_list_;

    class EventCallbackHolderBase {
     public:
        virtual ~EventCallbackHolderBase() {}
        virtual void operator()(const std::string &) = 0;
    };

    template <typename EventT>
    class EventCallbackHolder : public EventCallbackHolderBase {
     public:
        EventCallbackHolder(std::function<void(const EventT &)> callback) : callback_(callback) {}
        virtual void operator()(const std::string &data) {
            EventT event;
            event.unserialize(data);
            callback_(event);
        }

     private:
        std::function<void(const EventT &)> callback_;
    };

    struct SubEventInfo {
        std::string topic;
        uint32_t type = 0;
        std::shared_ptr<EventCallbackHolderBase> callback;
        int32_t socket = 0;
        std::string publisher;
    };
    std::unordered_map<std::string, SubEventInfo> sub_list_;
    std::queue<std::string> unprocessed_new_sub_events_;
    std::queue<std::string> unprocessed_dead_sub_events_;

    class MethodCallbackHolderBase {
     public:
        virtual ~MethodCallbackHolderBase() {}
        virtual void operator()(const std::string &, std::string *) = 0;
    };

    template <typename RequestT, typename ResponseT>
    class MethodCallbackHolder : public MethodCallbackHolderBase {
     public:
        MethodCallbackHolder(std::function<void(const RequestT &, ResponseT *)> callback) : callback_(callback) {}
        virtual void operator()(const std::string &request, std::string *response) {
            if (response == nullptr) {
                LERROR(UBusRuntime) << "Error nullptr" << std::endl;
                return;
            }
            RequestT req;
            req.unserialize(request);
            ResponseT resp;
            callback_(req, &resp);
            resp.serialize(response);
        }

     private:
        std::function<void(const RequestT &, ResponseT *)> callback_;
    };

    struct MethodInfo {
        std::string method;
        uint32_t request_type = 0;
        uint32_t response_type = 0;
        std::shared_ptr<MethodCallbackHolderBase> callback;
    };
    std::unordered_map<std::string, MethodInfo> method_list_;

    const uint32_t max_connections_ = 1024;

    std::string name_;

 private:
    void keep_alive_sender();
    void start_listening_socket();
    void process_event_message();
};

template <typename EventT>
bool UBusRuntime::advertise_event(const std::string &topic) {
    Frame frame;
    frame.header.message_type = FRAME_EVENT_REGISTER;
    nlohmann::json json_struct;
    json_struct["topic"] = topic;
    json_struct["type_id"] = EventT::id;
    std::string serialized_string = json_struct.dump();
    const char *char_struct = serialized_string.c_str();
    frame.header.data_length = htonl(static_cast<uint32_t>(serialized_string.size()));
    LDEBUG(UBusRuntime) << "Data length : " << ntohl(frame.header.data_length) << std::endl;
    frame.data = new uint8_t[ntohl(frame.header.data_length)];
    strncpy(reinterpret_cast<char *>(frame.data), char_struct, ntohl(frame.header.data_length));
    int32_t ret;
    if ((ret = writen(control_sock_, static_cast<void *>(&frame.header), sizeof(FrameHeader))) < 0) {
        LINFO(UBusRuntime) << "Write returned " << ret << std::endl;
    }
    if ((ret = writen(control_sock_, static_cast<void *>(frame.data), ntohl(frame.header.data_length))) < 0) {
        LINFO(UBusRuntime) << "Write returned " << ret << std::endl;
    }
    LDEBUG(UBusRuntime) << "Debug content "
                        << std::string(reinterpret_cast<char *>(frame.data), ntohl(frame.header.data_length))
                        << std::endl;

    {
        char header_buff[sizeof(FrameHeader)];
        size_t read_size = readn(control_sock_, &header_buff, sizeof(FrameHeader));
        if (read_size < sizeof(FrameHeader)) {
            LERROR(UBusRuntime) << "Failed to read header" << std::endl;
        } else {
            FrameHeader *header = reinterpret_cast<FrameHeader *>(header_buff);
            header->data_length = ntohl(header->data_length);
            LDEBUG(UBusRuntime) << "Size of data to read : " << header->data_length << std::endl;
            char content_buff[header->data_length];
            read_size = readn(control_sock_, &content_buff, header->data_length);
            if (read_size < header->data_length) {
                LERROR(UBusRuntime) << "Failed to read content" << std::endl;
            }
            std::string content(content_buff, header->data_length);
            try {
                nlohmann::json response_json = nlohmann::json::parse(content);
                if (response_json.contains("response")) {
                    if (response_json["response"] == "OK") {
                        LINFO(UBusRuntime) << "Topic registered to master" << std::endl;
                        PubEventInfo event_info;
                        event_info.topic = topic;
                        event_info.type = EventT::id;
                        pub_list_[topic] = event_info;
                    } else {
                        LERROR(UBusRuntime) << "Error from master : " << response_json["response"] << std::endl;
                        return false;
                    }
                } else {
                    LERROR(UBusRuntime) << "Invalid response from master" << std::endl;
                    return false;
                }
            } catch (nlohmann::json::exception &e) {
                LERROR(UBusRuntime) << "Exception in json : " << e.what() << std::endl;
                return false;
            }
        }
    }
    return true;
}

template <typename EventT>
bool UBusRuntime::publish_event(const std::string &topic, const EventT &event) {
    if (pub_list_.find(topic) == pub_list_.end()) {
        LERROR(UBusRuntime) << "Error topic unregistered" << std::endl;
        return false;
    }
    if (EventT::id != pub_list_.at(topic).type) {
        LERROR(UBusRuntime) << "Error wrong event type" << std::endl;
        return false;
    }
    if (pub_list_.at(topic).client_socket_map.size() == 0) {
        LINFO(UBusRuntime) << "No subscribers" << std::endl;
        return true;
    }
    Frame frame;
    frame.header.message_type = FRAME_EVENT;
    std::string serialized_string;
    event.serialize(&serialized_string);
    const char *char_struct = serialized_string.c_str();
    frame.header.data_length = htonl(static_cast<uint32_t>(serialized_string.size()));
    frame.data = new uint8_t[serialized_string.size()];
    strncpy(reinterpret_cast<char *>(frame.data), char_struct, serialized_string.size());
    for (auto &p : pub_list_.at(topic).client_socket_map) {
        LINFO(UBusRuntime) << "Sending event to subscriber " << p.first << std::endl;
        int32_t ret;
        if ((ret = writen(p.second, static_cast<void *>(&frame.header), sizeof(FrameHeader))) < 0) {
            LDEBUG(UBusRuntime) << "Write returned " << ret << std::endl;
        }
        if ((ret = writen(p.second, static_cast<void *>(frame.data), serialized_string.size())) < 0) {
            LDEBUG(UBusRuntime) << "Write returned " << ret << std::endl;
        }
    }
    return true;
}

template <typename EventT>
bool UBusRuntime::subscribe_event(const std::string &topic, std::function<void(const EventT &)> callback) {
    Frame frame;
    frame.header.message_type = FRAME_EVENT_SUBSCRIBE;

    nlohmann::json json_struct;
    json_struct["topic"] = topic;
    json_struct["type_id"] = EventT::id;
    json_struct["name"] = name_;
    std::string serialized_string = json_struct.dump();

    const char *char_struct = serialized_string.c_str();
    frame.header.data_length = htonl(static_cast<uint32_t>(serialized_string.size()));
    frame.data = new uint8_t[serialized_string.size()];
    strncpy(reinterpret_cast<char *>(frame.data), char_struct, serialized_string.size());
    int32_t ret;
    if ((ret = writen(control_sock_, static_cast<void *>(&frame.header), sizeof(FrameHeader))) < 0) {
        LINFO(UBusRuntime) << "Write returned " << ret << std::endl;
    }
    if ((ret = writen(control_sock_, static_cast<void *>(frame.data), serialized_string.size())) < 0) {
        LINFO(UBusRuntime) << "Write returned " << ret << std::endl;
    }

    {
        char header_buff[sizeof(FrameHeader)];
        size_t read_size = readn(control_sock_, &header_buff, sizeof(FrameHeader));
        if (read_size < sizeof(FrameHeader)) {
            LERROR(UBusRuntime) << "Failed to read header" << std::endl;
        } else {
            FrameHeader *header = reinterpret_cast<FrameHeader *>(header_buff);
            header->data_length = ntohl(header->data_length);
            LDEBUG(UBusRuntime) << "Size of data to read : " << header->data_length << std::endl;
            char content_buff[header->data_length];
            read_size = readn(control_sock_, &content_buff, header->data_length);
            if (read_size < header->data_length) {
                LERROR(UBusRuntime) << "Failed to read content" << std::endl;
            }
            std::string content(content_buff, header->data_length);
            try {
                nlohmann::json response_json = nlohmann::json::parse(content);
                if (response_json.contains("response")) {
                    if (response_json["response"] == "OK") {
                        LINFO(UBusRuntime) << "Topic subscription registered to master" << std::endl;
                        if (!response_json.contains("publisher_ip") || !response_json.contains("publisher_port") ||
                            !response_json.contains("publisher_name")) {
                            LERROR(UBusRuntime) << "Invalid reponse from master for subscription" << std::endl;
                            return false;
                        }
                        int32_t sub_socket = 0;
                        if ((sub_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                            LERROR(UBusRuntime) << "Failed to create socket" << std::endl;
                            control_sock_ = 0;
                            return false;
                        }
                        sockaddr_in sub_sockaddr;
                        bzero(&sub_sockaddr, sizeof(sub_sockaddr));
                        sub_sockaddr.sin_family = AF_INET;
                        sub_sockaddr.sin_port = htons(response_json.at("publisher_port").get<int32_t>());
                        if (inet_pton(AF_INET, response_json.at("publisher_ip").get<std::string>().c_str(),
                                      &sub_sockaddr.sin_addr) <= 0) {
                            LERROR(UBusRuntime)
                                << "Failed to convert ip address " << response_json.at("publisher_ip") << std::endl;
                            return false;
                        }

                        int32_t ret;
                        if ((ret = connect(sub_socket, reinterpret_cast<sockaddr *>(&sub_sockaddr),
                                           sizeof(sub_sockaddr))) != 0) {
                            LERROR(UBusRuntime) << "Failed to connect to publisher, ret " << ret << std::endl;
                            return false;
                        }

                        SubEventInfo event_info;
                        event_info.topic = topic;
                        event_info.type = EventT::id;
                        event_info.socket = sub_socket;
                        event_info.publisher = response_json.at("publisher_name").get<std::string>();
                        event_info.callback = std::make_shared<EventCallbackHolder<EventT> >(callback);

                        // send subscribe message to publisher
                        if ((ret = writen(sub_socket, static_cast<void *>(&frame.header), sizeof(FrameHeader))) < 0) {
                            LINFO(UBusRuntime) << "Write returned " << ret << std::endl;
                        }
                        if ((ret = writen(sub_socket, static_cast<void *>(frame.data),
                                          ntohl(frame.header.data_length))) < 0) {
                            LINFO(UBusRuntime) << "Write returned " << ret << std::endl;
                        }

                        {
                            char header_buff[sizeof(FrameHeader)];
                            size_t read_size = readn(sub_socket, &header_buff, sizeof(FrameHeader));
                            if (read_size < sizeof(FrameHeader)) {
                                LERROR(UBusRuntime) << "Failed to read header" << std::endl;
                            } else {
                                FrameHeader *header = reinterpret_cast<FrameHeader *>(header_buff);
                                header->data_length = ntohl(header->data_length);
                                LDEBUG(UBusRuntime) << "Size of data to read : " << header->data_length << std::endl;
                                char content_buff[header->data_length];
                                read_size = readn(sub_socket, &content_buff, header->data_length);
                                if (read_size < header->data_length) {
                                    LDEBUG(UBusRuntime) << "Failed to read content" << std::endl;
                                }
                                std::string content(content_buff, header->data_length);
                                try {
                                    nlohmann::json response_json = nlohmann::json::parse(content);
                                    if (response_json.contains("response")) {
                                        if (response_json["response"] == "OK") {
                                            LINFO(UBusRuntime) << "Registered with publisher" << std::endl;
                                            sub_list_[topic] = event_info;
                                            unprocessed_new_sub_events_.push(topic);
                                            return true;
                                        } else {
                                            LERROR(UBusRuntime) << "Failed to connect to "
                                                                   "publisher"
                                                                << std::endl;
                                            return false;
                                        }
                                    } else {
                                        LERROR(UBusRuntime) << "Failed to connect to publisher" << std::endl;
                                        return false;
                                    }
                                } catch (nlohmann::json::exception &e) {
                                    LERROR(UBusRuntime) << "Exception in json : " << e.what() << std::endl;
                                    return false;
                                }
                            }
                        }

                    } else {
                        LERROR(UBusRuntime) << "Error from master : " << response_json["response"] << std::endl;
                        return false;
                    }
                } else {
                    LERROR(UBusRuntime) << "Invalid response from master" << std::endl;
                    return false;
                }
            } catch (nlohmann::json::exception &e) {
                LERROR(UBusRuntime) << "Exception in json : " << e.what() << std::endl;
                return false;
            }
        }
    }
    return true;
}

template <typename RequestT, typename ResponseT>
bool UBusRuntime::provide_method(const std::string &method,
                                 std::function<void(const RequestT &, ResponseT *)> callback) {
    Frame frame;
    frame.header.message_type = FRAME_METHOD_PROVIDE;
    nlohmann::json json_struct;
    json_struct["method"] = method;
    json_struct["request_type_id"] = RequestT::id;
    json_struct["response_type_id"] = ResponseT::id;
    std::string serialized_string = json_struct.dump();
    const char *char_struct = serialized_string.c_str();
    frame.header.data_length = htonl(static_cast<uint32_t>(serialized_string.size()));
    LDEBUG(UBusRuntime) << "Data length : " << ntohl(frame.header.data_length) << std::endl;
    frame.data = new uint8_t[ntohl(frame.header.data_length)];
    strncpy(reinterpret_cast<char *>(frame.data), char_struct, serialized_string.size());
    int32_t ret;
    if ((ret = writen(control_sock_, static_cast<void *>(&frame.header), sizeof(FrameHeader))) < 0) {
        LINFO(UBusRuntime) << "Write returned " << ret << std::endl;
    }
    if ((ret = writen(control_sock_, static_cast<void *>(frame.data), ntohl(frame.header.data_length))) < 0) {
        LINFO(UBusRuntime) << "Write returned " << ret << std::endl;
    }
    LDEBUG(UBusRuntime) << "Debug content "
                        << std::string(reinterpret_cast<char *>(frame.data), ntohl(frame.header.data_length))
                        << std::endl;

    {
        char header_buff[sizeof(FrameHeader)];
        size_t read_size = readn(control_sock_, &header_buff, sizeof(FrameHeader));
        if (read_size < sizeof(FrameHeader)) {
            LERROR(UBusRuntime) << "Failed to read header" << std::endl;
        } else {
            FrameHeader *header = reinterpret_cast<FrameHeader *>(header_buff);
            header->data_length = ntohl(header->data_length);
            LDEBUG(UBusRuntime) << "Size of data to read : " << header->data_length << std::endl;
            char content_buff[header->data_length];
            read_size = readn(control_sock_, &content_buff, header->data_length);
            if (read_size < header->data_length) {
                LERROR(UBusRuntime) << "Failed to read content" << std::endl;
            }
            std::string content(content_buff, header->data_length);
            try {
                nlohmann::json response_json = nlohmann::json::parse(content);
                if (response_json.contains("response")) {
                    if (response_json["response"] == "OK") {
                        LINFO(UBusRuntime) << "Method registered to master" << std::endl;
                        MethodInfo method_info;
                        method_info.method = method;
                        method_info.request_type = RequestT::id;
                        method_info.response_type = ResponseT::id;
                        method_info.callback = std::make_shared<MethodCallbackHolder<RequestT, ResponseT> >(callback);
                        method_list_[method] = method_info;
                    } else {
                        LERROR(UBusRuntime) << "Error from master : " << response_json["response"] << std::endl;
                        return false;
                    }
                } else {
                    LERROR(UBusRuntime) << "Invalid response from master" << std::endl;
                    return false;
                }
            } catch (nlohmann::json::exception &e) {
                LERROR(UBusRuntime) << "Exception in json : " << e.what() << std::endl;
                return false;
            }
        }
    }
    return true;
}

template <typename RequestT, typename ResponseT>
bool UBusRuntime::call_method(const std::string &method, const RequestT &request, ResponseT *response) {
    // get method info
    Frame frame;
    frame.header.message_type = FRAME_METHOD_QUERY;

    nlohmann::json json_struct;
    json_struct["method"] = method;
    json_struct["request_type_id"] = RequestT::id;
    json_struct["response_type_id"] = ResponseT::id;
    json_struct["name"] = name_;
    std::string serialized_string = json_struct.dump();

    const char *char_struct = serialized_string.c_str();
    frame.header.data_length = htonl(static_cast<uint32_t>(serialized_string.size()));
    frame.data = new uint8_t[ntohl(frame.header.data_length)];
    strncpy(reinterpret_cast<char *>(frame.data), char_struct, serialized_string.size());
    int32_t ret;
    if ((ret = writen(control_sock_, static_cast<void *>(&frame.header), sizeof(FrameHeader))) < 0) {
        LINFO(UBusRuntime) << "Write returned " << ret << std::endl;
    }
    if ((ret = writen(control_sock_, static_cast<void *>(frame.data), ntohl(frame.header.data_length))) < 0) {
        LINFO(UBusRuntime) << "Write returned " << ret << std::endl;
    }

    {
        // get provider info
        char header_buff[sizeof(FrameHeader)];
        size_t read_size = readn(control_sock_, &header_buff, sizeof(FrameHeader));
        if (read_size < sizeof(FrameHeader)) {
            LERROR(UBusRuntime) << "Failed to read header" << std::endl;
            return false;
        }
        FrameHeader *header = reinterpret_cast<FrameHeader *>(header_buff);
        header->data_length = ntohl(header->data_length);
        LDEBUG(UBusRuntime) << "Size of data to read : " << header->data_length << std::endl;
        char content_buff[header->data_length];
        read_size = readn(control_sock_, &content_buff, header->data_length);
        if (read_size < header->data_length) {
            LERROR(UBusRuntime) << "Failed to read content" << std::endl;
        }
        std::string content(content_buff, header->data_length);

        try {
            nlohmann::json response_json = nlohmann::json::parse(content);
            if (!response_json.contains("response")) {
                LERROR(UBusRuntime) << "Invalid response from master" << std::endl;
                return false;
            }
            if (response_json["response"] != "OK") {
                LERROR(UBusRuntime) << "Master returned " << response_json["response"] << std::endl;
                return false;
            }
            LINFO(UBusRuntime) << "Method request is validated by master." << std::endl;
            if (!response_json.contains("provider_ip") || !response_json.contains("provider_port") ||
                !response_json.contains("provider_name")) {
                LERROR(UBusRuntime) << "Invalid reponse from master for method request" << std::endl;
                return false;
            }
            int32_t req_socket = 0;
            if ((req_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                LERROR(UBusRuntime) << "Failed to create socket" << std::endl;
                control_sock_ = 0;
                return false;
            }
            sockaddr_in req_sockaddr;
            bzero(&req_sockaddr, sizeof(req_sockaddr));
            req_sockaddr.sin_family = AF_INET;
            req_sockaddr.sin_port = htons(response_json.at("provider_port").get<int32_t>());
            if (inet_pton(AF_INET, response_json.at("provider_ip").get<std::string>().c_str(),
                          &req_sockaddr.sin_addr) <= 0) {
                LERROR(UBusRuntime) << "Failed to convert ip address " << response_json.at("provider_ip") << std::endl;
                return false;
            }

            int32_t ret_connect;
            if ((ret_connect =
                     connect(req_socket, reinterpret_cast<sockaddr *>(&req_sockaddr), sizeof(req_sockaddr))) != 0) {
                LERROR(UBusRuntime) << "Failed to connect to method provider, ret " << ret_connect << std::endl;
                return false;
            }

            Frame req_frame;
            req_frame.header.message_type = FRAME_METHOD_CALL;
            std::string request_string;
            request.serialize(&request_string);
            nlohmann::json method_req_json;
            method_req_json["method"] = method;
            method_req_json["request_type_id"] = RequestT::id;
            method_req_json["response_type_id"] = ResponseT::id;
            method_req_json["name"] = name_;
            method_req_json["request_data"] = request_string;
            std::string serialized_string = method_req_json.dump();
            const char *char_struct = serialized_string.c_str();
            req_frame.header.data_length = htonl(static_cast<uint32_t>(serialized_string.size()));
            req_frame.data = new uint8_t[ntohl(req_frame.header.data_length)];
            strncpy(reinterpret_cast<char *>(req_frame.data), char_struct, serialized_string.size());

            int32_t ret;
            if ((ret = writen(req_socket, static_cast<void *>(&req_frame.header), sizeof(FrameHeader))) < 0) {
                LDEBUG(UBusRuntime) << "Write returned " << ret << std::endl;
            }
            if ((ret = writen(req_socket, static_cast<void *>(req_frame.data), ntohl(req_frame.header.data_length))) <
                0) {
                LDEBUG(UBusRuntime) << "Write returned " << ret << std::endl;
            }

            // get response
            {
                char header_buff[sizeof(FrameHeader)];
                size_t read_size = readn(req_socket, &header_buff, sizeof(FrameHeader));
                if (read_size < sizeof(FrameHeader)) {
                    LERROR(UBusRuntime) << "Failed to read header" << std::endl;
                    return false;
                }
                FrameHeader *header = reinterpret_cast<FrameHeader *>(header_buff);
                header->data_length = ntohl(header->data_length);
                if (header->message_type != FRAME_METHOD_RESPONSE) {
                    LERROR(UBusRuntime) << "Invalid frame type" << std::endl;
                    return false;
                }
                char content_buff[header->data_length];
                read_size = readn(req_socket, &content_buff, header->data_length);
                if (read_size < header->data_length) {
                    LERROR(UBusRuntime) << "Failed to read content" << std::endl;
                }
                std::string content(content_buff, header->data_length);
                try {
                    nlohmann::json response_json = nlohmann::json::parse(content);
                    if (response_json.contains("response")) {
                        if (response_json["response"] == "OK") {
                            LINFO(UBusRuntime) << "Method registered to master" << std::endl;
                            if (!response_json.contains("response_data")) {
                                LERROR(UBusRuntime) << "Invalid frame format" << std::endl;
                                return false;
                            }
                            response->unserialize(response_json.at("response_data"));
                        } else {
                            LERROR(UBusRuntime) << "Error request method : " << response_json["response"] << std::endl;
                            return false;
                        }
                    } else {
                        LERROR(UBusRuntime) << "Invalid frame format" << std::endl;
                        return false;
                    }
                } catch (nlohmann::json::exception &e) {
                    LERROR(UBusRuntime) << "Exception in json : " << e.what() << std::endl;
                    return false;
                }
            }
        } catch (nlohmann::json::exception &e) {
            LERROR(UBusRuntime) << "Exception in json : " << e.what() << std::endl;
            return false;
        }
    }
    return true;
}
