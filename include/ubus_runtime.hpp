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

// class Publisher {
//  public:
//     template <typename EventT>
//     bool publish(const EventT &data);
// };

class UBusRuntime {
 public:
    bool init(const std::string &name, const std::string &ip, uint32_t port);

    template <typename EventT>
    bool subscribe_event(const std::string &topic,
                         std::function<void(const EventT &)> callback);

    template <typename EventT>
    bool advertise_event(const std::string &topic);

    template <typename EventT>
    bool publish_event(const std::string &topic, const EventT &event);

    template <typename MethodT>
    bool call_method(const std::string &method, MethodT *data);

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

    class CallbackHolderBase {
     public:
        virtual ~CallbackHolderBase() {}
        virtual void operator()(const std::string &) = 0;
    };

    template <typename EventT>
    class CallbackHolder : public CallbackHolderBase {
     public:
        CallbackHolder(std::function<void(const EventT &)> callback)
            : callback_(callback) {}
        virtual void operator()(const std::string &data) {
            EventT event;
            event.unserilize(data);
            callback_(event);
        }

     private:
        std::function<void(const EventT &)> callback_;
    };

    struct SubEventInfo {
        std::string topic;
        uint32_t type = 0;
        std::shared_ptr<CallbackHolderBase> callback;
        int32_t socket = 0;
        std::string publisher;
    };
    std::unordered_map<std::string, SubEventInfo> sub_list_;
    std::queue<std::string> unprocessed_new_sub_events_;
    std::queue<std::string> unprocessed_dead_sub_events_;

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
    frame.header.message_type = FRAME_PUBLISH;
    nlohmann::json json_struct;
    json_struct["topic"] = topic;
    json_struct["type_id"] = EventT::id;
    std::string serilized_string = json_struct.dump();
    const char *char_struct = serilized_string.c_str();
    frame.header.data_length = serilized_string.size();
    LOG(UBusRuntime) << "Data length : " << frame.header.data_length
                     << std::endl;
    frame.data = new uint8_t[frame.header.data_length];
    strncpy(reinterpret_cast<char *>(frame.data), char_struct,
            serilized_string.size());
    int32_t ret;
    if ((ret = writen(control_sock_, static_cast<void *>(&frame.header),
                      sizeof(FrameHeader))) < 0) {
        LOG(UBusRuntime) << "Write returned " << ret << std::endl;
    }
    if ((ret = writen(control_sock_, static_cast<void *>(frame.data),
                      frame.header.data_length)) < 0) {
        LOG(UBusRuntime) << "Write returned " << ret << std::endl;
    }
    LOG(UBusRuntime) << "Debug content "
                     << std::string(reinterpret_cast<char *>(frame.data),
                                    frame.header.data_length)
                     << std::endl;

    {
        char header_buff[sizeof(FrameHeader)];
        size_t read_size =
            readn(control_sock_, &header_buff, sizeof(FrameHeader));
        if (read_size < sizeof(FrameHeader)) {
            LOG(UBusRuntime) << "Failed to read header" << std::endl;
        } else {
            FrameHeader *header = reinterpret_cast<FrameHeader *>(header_buff);
            LOG(UBusRuntime) << "Size of data to read : " << header->data_length
                             << std::endl;
            char content_buff[header->data_length];
            read_size =
                readn(control_sock_, &content_buff, header->data_length);
            if (read_size < header->data_length) {
                LOG(UBusRuntime) << "Failed to read content" << std::endl;
            }
            std::string content(content_buff, header->data_length);
            try {
                nlohmann::json response_json = nlohmann::json::parse(content);
                if (response_json.contains("response")) {
                    if (response_json["response"] == "OK") {
                        LOG(UBusRuntime)
                            << "Topic registered to master" << std::endl;
                        PubEventInfo event_info;
                        event_info.topic = topic;
                        event_info.type = EventT::id;
                        pub_list_[topic] = event_info;
                    } else {
                        LOG(UBusRuntime)
                            << "Error from master : "
                            << response_json["response"] << std::endl;
                        return false;
                    }
                } else {
                    LOG(UBusRuntime)
                        << "Invalid response from master" << std::endl;
                    return false;
                }
            } catch (nlohmann::json::exception &e) {
                LOG(UBusRuntime)
                    << "Exception in json : " << e.what() << std::endl;
                return false;
            }
        }
    }
    return true;
}

template <typename EventT>
bool UBusRuntime::publish_event(const std::string &topic, const EventT &event) {
    if (pub_list_.find(topic) == pub_list_.end()) {
        LOG(UBusRuntime) << "Error topic unregistered" << std::endl;
        return false;
    }
    if (EventT::id != pub_list_.at(topic).type) {
        LOG(UBusRuntime) << "Error wrong event type" << std::endl;
        return false;
    }
    if (pub_list_.at(topic).client_socket_map.size() == 0) {
        LOG(UBusRuntime) << "No subscribers" << std::endl;
        return true;
    }
    Frame frame;
    frame.header.message_type = FRAME_EVENT;
    std::string serilized_string;
    event.serilize(&serilized_string);
    const char *char_struct = serilized_string.c_str();
    frame.header.data_length = serilized_string.size();
    frame.data = new uint8_t[frame.header.data_length];
    strncpy(reinterpret_cast<char *>(frame.data), char_struct,
            serilized_string.size());
    for (auto &p : pub_list_.at(topic).client_socket_map) {
        LOG(UBusRuntime) << "Sending event to subscriber " << p.first
                         << std::endl;
        int32_t ret;
        if ((ret = writen(p.second, static_cast<void *>(&frame.header),
                          sizeof(FrameHeader))) < 0) {
            LOG(UBusRuntime) << "Write returned " << ret << std::endl;
        }
        if ((ret = writen(p.second, static_cast<void *>(frame.data),
                          frame.header.data_length)) < 0) {
            LOG(UBusRuntime) << "Write returned " << ret << std::endl;
        }
    }
    return true;
}

template <typename EventT>
bool UBusRuntime::subscribe_event(
    const std::string &topic, std::function<void(const EventT &)> callback) {
    Frame frame;
    frame.header.message_type = FRAME_SUBSCRIBE;

    nlohmann::json json_struct;
    json_struct["topic"] = topic;
    json_struct["type_id"] = EventT::id;
    json_struct["name"] = name_;
    std::string serilized_string = json_struct.dump();

    const char *char_struct = serilized_string.c_str();
    frame.header.data_length = serilized_string.size();
    frame.data = new uint8_t[frame.header.data_length];
    strncpy(reinterpret_cast<char *>(frame.data), char_struct,
            serilized_string.size());
    int32_t ret;
    if ((ret = writen(control_sock_, static_cast<void *>(&frame.header),
                      sizeof(FrameHeader))) < 0) {
        LOG(UBusRuntime) << "Write returned " << ret << std::endl;
    }
    if ((ret = writen(control_sock_, static_cast<void *>(frame.data),
                      frame.header.data_length)) < 0) {
        LOG(UBusRuntime) << "Write returned " << ret << std::endl;
    }

    {
        char header_buff[sizeof(FrameHeader)];
        size_t read_size =
            readn(control_sock_, &header_buff, sizeof(FrameHeader));
        if (read_size < sizeof(FrameHeader)) {
            LOG(UBusRuntime) << "Failed to read header" << std::endl;
        } else {
            FrameHeader *header = reinterpret_cast<FrameHeader *>(header_buff);
            LOG(UBusRuntime) << "Size of data to read : " << header->data_length
                             << std::endl;
            char content_buff[header->data_length];
            read_size =
                readn(control_sock_, &content_buff, header->data_length);
            if (read_size < header->data_length) {
                LOG(UBusRuntime) << "Failed to read content" << std::endl;
            }
            std::string content(content_buff, header->data_length);
            try {
                nlohmann::json response_json = nlohmann::json::parse(content);
                if (response_json.contains("response")) {
                    if (response_json["response"] == "OK") {
                        LOG(UBusRuntime)
                            << "Topic subscription registered to master"
                            << std::endl;
                        if (!response_json.contains("publisher_ip") ||
                            !response_json.contains("publisher_port") ||
                            !response_json.contains("publisher_name")) {
                            LOG(UBusRuntime) << "Invalid reponse from master "
                                                "for subscription"
                                             << std::endl;
                            return false;
                        }
                        int32_t sub_socket = 0;
                        if ((sub_socket = socket(AF_INET, SOCK_STREAM, 0)) <
                            0) {
                            LOG(UBusRuntime)
                                << "Failed to create socket" << std::endl;
                            control_sock_ = 0;
                            return false;
                        }
                        sockaddr_in sub_sockaddr;
                        bzero(&sub_sockaddr, sizeof(sub_sockaddr));
                        sub_sockaddr.sin_family = AF_INET;
                        sub_sockaddr.sin_port = htons(
                            response_json.at("publisher_port").get<int32_t>());
                        if (inet_pton(AF_INET,
                                      response_json.at("publisher_ip")
                                          .get<std::string>()
                                          .c_str(),
                                      &sub_sockaddr.sin_addr) <= 0) {
                            LOG(UBusRuntime) << "Failed to convert ip address "
                                             << response_json.at("publisher_ip")
                                             << std::endl;
                            return false;
                        }

                        if (connect(sub_socket,
                                    reinterpret_cast<sockaddr *>(&sub_sockaddr),
                                    sizeof(sub_sockaddr)) != 0) {
                            LOG(UBusRuntime) << "Failed to connect to publisher"
                                             << std::endl;
                            return false;
                        }

                        SubEventInfo event_info;
                        event_info.topic = topic;
                        event_info.type = EventT::id;
                        event_info.socket = sub_socket;
                        event_info.publisher =
                            response_json.at("publisher_name")
                                .get<std::string>();
                        event_info.callback =
                            std::make_shared<CallbackHolder<EventT> >(callback);

                        // send subscribe message to publisher
                        if ((ret = writen(sub_socket,
                                          static_cast<void *>(&frame.header),
                                          sizeof(FrameHeader))) < 0) {
                            LOG(UBusRuntime)
                                << "Write returned " << ret << std::endl;
                        }
                        if ((ret = writen(sub_socket,
                                          static_cast<void *>(frame.data),
                                          frame.header.data_length)) < 0) {
                            LOG(UBusRuntime)
                                << "Write returned " << ret << std::endl;
                        }

                        {
                            char header_buff[sizeof(FrameHeader)];
                            size_t read_size = readn(sub_socket, &header_buff,
                                                     sizeof(FrameHeader));
                            if (read_size < sizeof(FrameHeader)) {
                                LOG(UBusRuntime)
                                    << "Failed to read header" << std::endl;
                            } else {
                                FrameHeader *header =
                                    reinterpret_cast<FrameHeader *>(
                                        header_buff);
                                LOG(UBusRuntime)
                                    << "Size of data to read : "
                                    << header->data_length << std::endl;
                                char content_buff[header->data_length];
                                read_size = readn(sub_socket, &content_buff,
                                                  header->data_length);
                                if (read_size < header->data_length) {
                                    LOG(UBusRuntime) << "Failed to read content"
                                                     << std::endl;
                                }
                                std::string content(content_buff,
                                                    header->data_length);
                                try {
                                    nlohmann::json response_json =
                                        nlohmann::json::parse(content);
                                    if (response_json.contains("response")) {
                                        if (response_json["response"] == "OK") {
                                            LOG(UBusRuntime)
                                                << "Registered with publisher"
                                                << std::endl;
                                            sub_list_[topic] = event_info;
                                            unprocessed_new_sub_events_.push(
                                                topic);
                                            return true;
                                        } else {
                                            LOG(UBusRuntime)
                                                << "Failed to connect to "
                                                   "publisher"
                                                << std::endl;
                                            return false;
                                        }
                                    } else {
                                        LOG(UBusRuntime)
                                            << "Failed to connect to publisher"
                                            << std::endl;
                                        return false;
                                    }
                                } catch (nlohmann::json::exception &e) {
                                    LOG(UBusRuntime)
                                        << "Exception in json : " << e.what()
                                        << std::endl;
                                    return false;
                                }
                            }
                        }

                    } else {
                        LOG(UBusRuntime)
                            << "Error from master : "
                            << response_json["response"] << std::endl;
                        return false;
                    }
                } else {
                    LOG(UBusRuntime)
                        << "Invalid response from master" << std::endl;
                    return false;
                }
            } catch (nlohmann::json::exception &e) {
                LOG(UBusRuntime)
                    << "Exception in json : " << e.what() << std::endl;
                return false;
            }
        }
    }
    return true;
}