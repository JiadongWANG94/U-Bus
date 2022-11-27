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

#include "message.hpp"
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

    bool is_initiated() { return this->initiated_.load(); }

 protected:
    std::atomic<bool> initiated_{false};
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
            event.deserialize(data);
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
                LERROR(UBusRuntime) << "Error nullptr";
                return;
            }
            RequestT req;
            req.deserialize(request);
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
    LDEBUG(UBusRuntime) << "Data length : " << ntohl(frame.header.data_length);
    frame.data = new uint8_t[ntohl(frame.header.data_length)];
    strncpy(reinterpret_cast<char *>(frame.data), char_struct, ntohl(frame.header.data_length));
    int32_t ret;
    if ((ret = writen(control_sock_, static_cast<void *>(&frame.header), sizeof(FrameHeader))) < 0) {
        LINFO(UBusRuntime) << "Write returned " << ret;
    }
    if ((ret = writen(control_sock_, static_cast<void *>(frame.data), ntohl(frame.header.data_length))) < 0) {
        LINFO(UBusRuntime) << "Write returned " << ret;
    }
    LDEBUG(UBusRuntime) << "Debug content "
                        << std::string(reinterpret_cast<char *>(frame.data), ntohl(frame.header.data_length));

    {
        char header_buff[sizeof(FrameHeader)];
        size_t read_size = readn(control_sock_, &header_buff, sizeof(FrameHeader));
        if (read_size < sizeof(FrameHeader)) {
            LERROR(UBusRuntime) << "Failed to read header";
        } else {
            FrameHeader *header = reinterpret_cast<FrameHeader *>(header_buff);
            header->data_length = ntohl(header->data_length);
            LDEBUG(UBusRuntime) << "Size of data to read : " << header->data_length;
            char content_buff[header->data_length];
            read_size = readn(control_sock_, &content_buff, header->data_length);
            if (read_size < header->data_length) {
                LERROR(UBusRuntime) << "Failed to read content";
            }
            std::string content(content_buff, header->data_length);
            try {
                nlohmann::json response_json = nlohmann::json::parse(content);
                if (response_json.contains("response")) {
                    if (response_json["response"] == "OK") {
                        LINFO(UBusRuntime) << "Topic registered to master";
                        PubEventInfo event_info;
                        event_info.topic = topic;
                        event_info.type = EventT::id;
                        pub_list_[topic] = event_info;
                    } else {
                        LERROR(UBusRuntime) << "Error from master : " << std::string(response_json["response"]);
                        return false;
                    }
                } else {
                    LERROR(UBusRuntime) << "Invalid response from master";
                    return false;
                }
            } catch (nlohmann::json::exception &e) {
                LERROR(UBusRuntime) << "Exception in json : " << e.what();
                return false;
            }
        }
    }
    return true;
}

template <typename EventT>
bool UBusRuntime::publish_event(const std::string &topic, const EventT &event) {
    if (pub_list_.find(topic) == pub_list_.end()) {
        LERROR(UBusRuntime) << "Error topic unregistered";
        return false;
    }
    if (EventT::id != pub_list_.at(topic).type) {
        LERROR(UBusRuntime) << "Error wrong event type";
        return false;
    }
    if (pub_list_.at(topic).client_socket_map.size() == 0) {
        LINFO(UBusRuntime) << "No subscribers";
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
        LINFO(UBusRuntime) << "Sending event to subscriber " << p.first;
        int32_t ret;
        if ((ret = writen(p.second, static_cast<void *>(&frame.header), sizeof(FrameHeader))) < 0) {
            LDEBUG(UBusRuntime) << "Write returned " << ret;
        }
        if ((ret = writen(p.second, static_cast<void *>(frame.data), serialized_string.size())) < 0) {
            LDEBUG(UBusRuntime) << "Write returned " << ret;
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
        LINFO(UBusRuntime) << "Write returned " << ret;
    }
    if ((ret = writen(control_sock_, static_cast<void *>(frame.data), serialized_string.size())) < 0) {
        LINFO(UBusRuntime) << "Write returned " << ret;
    }

    {
        char header_buff[sizeof(FrameHeader)];
        size_t read_size = readn(control_sock_, &header_buff, sizeof(FrameHeader));
        if (read_size < sizeof(FrameHeader)) {
            LERROR(UBusRuntime) << "Failed to read header";
        } else {
            FrameHeader *header = reinterpret_cast<FrameHeader *>(header_buff);
            header->data_length = ntohl(header->data_length);
            LDEBUG(UBusRuntime) << "Size of data to read : " << header->data_length;
            char content_buff[header->data_length];
            read_size = readn(control_sock_, &content_buff, header->data_length);
            if (read_size < header->data_length) {
                LERROR(UBusRuntime) << "Failed to read content";
            }
            std::string content(content_buff, header->data_length);
            try {
                nlohmann::json response_json = nlohmann::json::parse(content);
                if (response_json.contains("response")) {
                    if (response_json["response"] == "OK") {
                        LINFO(UBusRuntime) << "Topic subscription registered to master";
                        if (!response_json.contains("publisher_ip") || !response_json.contains("publisher_port") ||
                            !response_json.contains("publisher_name")) {
                            LERROR(UBusRuntime) << "Invalid reponse from master for subscription";
                            return false;
                        }
                        int32_t sub_socket = 0;
                        if ((sub_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                            LERROR(UBusRuntime) << "Failed to create socket";
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
                                << "Failed to convert ip address " << std::string(response_json.at("publisher_ip"));
                            return false;
                        }

                        int32_t ret;
                        if ((ret = connect(sub_socket, reinterpret_cast<sockaddr *>(&sub_sockaddr),
                                           sizeof(sub_sockaddr))) != 0) {
                            LERROR(UBusRuntime) << "Failed to connect to publisher, ret " << ret;
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
                            LINFO(UBusRuntime) << "Write returned " << ret;
                        }
                        if ((ret = writen(sub_socket, static_cast<void *>(frame.data),
                                          ntohl(frame.header.data_length))) < 0) {
                            LINFO(UBusRuntime) << "Write returned " << ret;
                        }

                        {
                            char header_buff[sizeof(FrameHeader)];
                            size_t read_size = readn(sub_socket, &header_buff, sizeof(FrameHeader));
                            if (read_size < sizeof(FrameHeader)) {
                                LERROR(UBusRuntime) << "Failed to read header";
                            } else {
                                FrameHeader *header = reinterpret_cast<FrameHeader *>(header_buff);
                                header->data_length = ntohl(header->data_length);
                                LDEBUG(UBusRuntime) << "Size of data to read : " << header->data_length;
                                char content_buff[header->data_length];
                                read_size = readn(sub_socket, &content_buff, header->data_length);
                                if (read_size < header->data_length) {
                                    LDEBUG(UBusRuntime) << "Failed to read content";
                                }
                                std::string content(content_buff, header->data_length);
                                try {
                                    nlohmann::json response_json = nlohmann::json::parse(content);
                                    if (response_json.contains("response")) {
                                        if (response_json["response"] == "OK") {
                                            LINFO(UBusRuntime) << "Registered with publisher";
                                            sub_list_[topic] = event_info;
                                            unprocessed_new_sub_events_.push(topic);
                                            return true;
                                        } else {
                                            LERROR(UBusRuntime) << "Failed to connect to "
                                                                   "publisher";
                                            return false;
                                        }
                                    } else {
                                        LERROR(UBusRuntime) << "Failed to connect to publisher";
                                        return false;
                                    }
                                } catch (nlohmann::json::exception &e) {
                                    LERROR(UBusRuntime) << "Exception in json : " << e.what();
                                    return false;
                                }
                            }
                        }

                    } else {
                        LERROR(UBusRuntime) << "Error from master : " << std::string(response_json["response"]);
                        return false;
                    }
                } else {
                    LERROR(UBusRuntime) << "Invalid response from master";
                    return false;
                }
            } catch (nlohmann::json::exception &e) {
                LERROR(UBusRuntime) << "Exception in json : " << e.what();
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
    LDEBUG(UBusRuntime) << "Data length : " << ntohl(frame.header.data_length);
    frame.data = new uint8_t[ntohl(frame.header.data_length)];
    strncpy(reinterpret_cast<char *>(frame.data), char_struct, serialized_string.size());
    int32_t ret;
    if ((ret = writen(control_sock_, static_cast<void *>(&frame.header), sizeof(FrameHeader))) < 0) {
        LINFO(UBusRuntime) << "Write returned " << ret;
    }
    if ((ret = writen(control_sock_, static_cast<void *>(frame.data), ntohl(frame.header.data_length))) < 0) {
        LINFO(UBusRuntime) << "Write returned " << ret;
    }
    LDEBUG(UBusRuntime) << "Debug content "
                        << std::string(reinterpret_cast<char *>(frame.data), ntohl(frame.header.data_length));

    {
        char header_buff[sizeof(FrameHeader)];
        size_t read_size = readn(control_sock_, &header_buff, sizeof(FrameHeader));
        if (read_size < sizeof(FrameHeader)) {
            LERROR(UBusRuntime) << "Failed to read header";
        } else {
            FrameHeader *header = reinterpret_cast<FrameHeader *>(header_buff);
            header->data_length = ntohl(header->data_length);
            LDEBUG(UBusRuntime) << "Size of data to read : " << header->data_length;
            char content_buff[header->data_length];
            read_size = readn(control_sock_, &content_buff, header->data_length);
            if (read_size < header->data_length) {
                LERROR(UBusRuntime) << "Failed to read content";
            }
            std::string content(content_buff, header->data_length);
            try {
                nlohmann::json response_json = nlohmann::json::parse(content);
                if (response_json.contains("response")) {
                    if (response_json["response"] == "OK") {
                        LINFO(UBusRuntime) << "Method registered to master";
                        MethodInfo method_info;
                        method_info.method = method;
                        method_info.request_type = RequestT::id;
                        method_info.response_type = ResponseT::id;
                        method_info.callback = std::make_shared<MethodCallbackHolder<RequestT, ResponseT> >(callback);
                        method_list_[method] = method_info;
                    } else {
                        LERROR(UBusRuntime) << "Error from master : " << std::string(response_json["response"]);
                        return false;
                    }
                } else {
                    LERROR(UBusRuntime) << "Invalid response from master";
                    return false;
                }
            } catch (nlohmann::json::exception &e) {
                LERROR(UBusRuntime) << "Exception in json : " << e.what();
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
        LINFO(UBusRuntime) << "Write returned " << ret;
    }
    if ((ret = writen(control_sock_, static_cast<void *>(frame.data), ntohl(frame.header.data_length))) < 0) {
        LINFO(UBusRuntime) << "Write returned " << ret;
    }

    {
        // get provider info
        char header_buff[sizeof(FrameHeader)];
        size_t read_size = readn(control_sock_, &header_buff, sizeof(FrameHeader));
        if (read_size < sizeof(FrameHeader)) {
            LERROR(UBusRuntime) << "Failed to read header";
            return false;
        }
        FrameHeader *header = reinterpret_cast<FrameHeader *>(header_buff);
        header->data_length = ntohl(header->data_length);
        LDEBUG(UBusRuntime) << "Size of data to read : " << header->data_length;
        char content_buff[header->data_length];
        read_size = readn(control_sock_, &content_buff, header->data_length);
        if (read_size < header->data_length) {
            LERROR(UBusRuntime) << "Failed to read content";
        }
        std::string content(content_buff, header->data_length);

        try {
            nlohmann::json response_json = nlohmann::json::parse(content);
            if (!response_json.contains("response")) {
                LERROR(UBusRuntime) << "Invalid response from master";
                return false;
            }
            if (response_json["response"] != "OK") {
                LERROR(UBusRuntime) << "Master returned " << std::string(response_json["response"]);
                return false;
            }
            LINFO(UBusRuntime) << "Method request is validated by master.";
            if (!response_json.contains("provider_ip") || !response_json.contains("provider_port") ||
                !response_json.contains("provider_name")) {
                LERROR(UBusRuntime) << "Invalid reponse from master for method request";
                return false;
            }
            int32_t req_socket = 0;
            if ((req_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                LERROR(UBusRuntime) << "Failed to create socket";
                control_sock_ = 0;
                return false;
            }
            sockaddr_in req_sockaddr;
            bzero(&req_sockaddr, sizeof(req_sockaddr));
            req_sockaddr.sin_family = AF_INET;
            req_sockaddr.sin_port = htons(response_json.at("provider_port").get<int32_t>());
            if (inet_pton(AF_INET, response_json.at("provider_ip").get<std::string>().c_str(),
                          &req_sockaddr.sin_addr) <= 0) {
                LERROR(UBusRuntime) << "Failed to convert ip address " << std::string(response_json.at("provider_ip"));
                return false;
            }

            int32_t ret_connect;
            if ((ret_connect =
                     connect(req_socket, reinterpret_cast<sockaddr *>(&req_sockaddr), sizeof(req_sockaddr))) != 0) {
                LERROR(UBusRuntime) << "Failed to connect to method provider, ret " << ret_connect;
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
                LDEBUG(UBusRuntime) << "Write returned " << ret;
            }
            if ((ret = writen(req_socket, static_cast<void *>(req_frame.data), ntohl(req_frame.header.data_length))) <
                0) {
                LDEBUG(UBusRuntime) << "Write returned " << ret;
            }

            // get response
            {
                char header_buff[sizeof(FrameHeader)];
                size_t read_size = readn(req_socket, &header_buff, sizeof(FrameHeader));
                if (read_size < sizeof(FrameHeader)) {
                    LERROR(UBusRuntime) << "Failed to read header";
                    return false;
                }
                FrameHeader *header = reinterpret_cast<FrameHeader *>(header_buff);
                header->data_length = ntohl(header->data_length);
                if (header->message_type != FRAME_METHOD_RESPONSE) {
                    LERROR(UBusRuntime) << "Invalid frame type";
                    return false;
                }
                char content_buff[header->data_length];
                read_size = readn(req_socket, &content_buff, header->data_length);
                if (read_size < header->data_length) {
                    LERROR(UBusRuntime) << "Failed to read content";
                }
                std::string content(content_buff, header->data_length);
                try {
                    nlohmann::json response_json = nlohmann::json::parse(content);
                    if (response_json.contains("response")) {
                        if (response_json["response"] == "OK") {
                            LINFO(UBusRuntime) << "Method registered to master";
                            if (!response_json.contains("response_data")) {
                                LERROR(UBusRuntime) << "Invalid frame format";
                                return false;
                            }
                            response->deserialize(response_json.at("response_data"));
                        } else {
                            LERROR(UBusRuntime) << "Error request method : " << std::string(response_json["response"]);
                            return false;
                        }
                    } else {
                        LERROR(UBusRuntime) << "Invalid frame format";
                        return false;
                    }
                } catch (nlohmann::json::exception &e) {
                    LERROR(UBusRuntime) << "Exception in json : " << e.what();
                    return false;
                }
            }
        } catch (nlohmann::json::exception &e) {
            LERROR(UBusRuntime) << "Exception in json : " << e.what();
            return false;
        }
    }
    return true;
}
