/**
 * Wang Jiadong <jiadong.wang.94@outlook.com>
 */

#include "ubus_master.hpp"

#include <poll.h>
#include <thread>

#include "nlohmann/json.hpp"

#include "helpers.hpp"
#include "frame.hpp"
#include "shared_lock_guard.hpp"

bool UBusMaster::init(const std::string &ip, uint32_t port) {
    if (this->initiated_.load()) {
        LWARN(UBusMaster) << "Already initiated";
        return false;
    }
    if ((control_sock_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LERROR(UBusMaster) << "Failed to create socket";
        control_sock_ = 0;
        return false;
    }
    sockaddr_in control_addr;
    bzero(&control_addr, sizeof(control_addr));
    control_addr.sin_family = AF_INET;
    control_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &control_addr.sin_addr) <= 0) {
        LERROR(UBusMaster) << "Failed to convert ip address " << ip;
        return false;
    }
    if (bind(control_sock_, reinterpret_cast<sockaddr *>(&control_addr), sizeof(control_addr)) < 0) {
        LERROR(UBusMaster) << "Failed to convert bind to ip " << ip << " port " << port;
        return false;
    }
    this->initiated_.store(true);
    return true;
}

bool UBusMaster::run() {
    std::thread message_worker(&UBusMaster::process_control_message, this);
    message_worker.detach();

    std::thread keep_alive_worker(&UBusMaster::keep_alive_worker, this);
    keep_alive_worker.detach();

    accept_new_connection();
    return true;
}

void UBusMaster::process_control_message() {
    pollfd poll_fd_list[max_connections_];
    for (int i = 0; i < max_connections_; ++i) {
        poll_fd_list[i].fd = -1;
    }

    while (1) {
        {
            std::lock_guard<std::mutex> lock(unprocessed_dead_participants_mtx_);
            while (!unprocessed_dead_participants_.empty()) {
                WritingSharedLockGuard shared_lock(participant_info_mtx_);
                auto &participant = unprocessed_dead_participants_.front();
                for (int i = 0; i < max_connections_; ++i) {
                    if (poll_fd_list[i].fd == participant_list_.at(participant)->socket) {
                        poll_fd_list[i].fd = -1;
                        break;
                    }
                }
                auto ite = participant_list_.find(participant);
                if (ite != participant_list_.end()) {
                    {
                        std::lock_guard<std::mutex> lock_event(event_list_mtx_);
                        for (auto &event : ite->second->published_topic_list) {
                            event_list_.erase(event.first);
                        }
                    }
                    {
                        std::lock_guard<std::mutex> lock_method(method_list_mtx_);
                        for (auto &method : ite->second->method_list) {
                            method_list_.erase(method.first);
                        }
                    }

                    auto socket = ite->second->socket;
                    participant_list_.erase(ite);
                    auto ite2 = socket_participant_mapping_.find(socket);
                    if (ite2 != socket_participant_mapping_.end()) {
                        socket_participant_mapping_.erase(ite2);
                    }
                }
                unprocessed_dead_participants_.pop();
            }
        }

        {
            std::lock_guard<std::mutex> lock(unprocessed_new_participants_mtx_);
            while (!unprocessed_new_participants_.empty()) {
                ReadingSharedLockGuard shared_lock(participant_info_mtx_);
                auto &participant = unprocessed_new_participants_.front();
                for (int i = 0; i < max_connections_; ++i) {
                    if (poll_fd_list[i].fd == -1) {
                        poll_fd_list[i].fd = participant_list_.at(participant)->socket;
                        poll_fd_list[i].events = POLLIN;
                        poll_fd_list[i].revents = 0;
                        break;
                    }
                }
                unprocessed_new_participants_.pop();
            }
        }
        int ret;
        {
            ReadingSharedLockGuard shared_lock(participant_info_mtx_);
            ret = poll(poll_fd_list, participant_list_.size(), 1000);
        }
        if (ret < 0) {
            LERROR(UBusMaster) << "Error in poll";
        } else if (ret == 0) {
            LTRACE(UBusMaster) << "Poll timeout";
        } else {
            for (int i = 0; i < max_connections_; ++i) {
                if (poll_fd_list[i].revents & POLLIN && poll_fd_list[i].fd >= 0) {
                    LTRACE(UBusMaster) << "Socket " << poll_fd_list[i].fd << " is readable";
                    LTRACE(UBusMaster) << "Revents is " << poll_fd_list[i].revents;
                    char header_buff[sizeof(FrameHeader)];
                    size_t read_size = read(poll_fd_list[i].fd, &header_buff, sizeof(FrameHeader));
                    if (read_size == 0) {
                        LWARN(UBusMaster) << "Peer is closed, remove from poll.";
                        poll_fd_list[i].fd = -1;
                        continue;
                    } else if (read_size < sizeof(FrameHeader)) {
                        LERROR(UBusMaster) << "Failed to read header, size of data read : " << read_size;
                        continue;
                    }
                    std::string content;
                    FrameHeader *header = reinterpret_cast<FrameHeader *>(header_buff);
                    header->data_length = ntohl(header->data_length);
                    if (header->data_length > 0) {
                        LDEBUG(UBusMaster) << "Size of data to read : " << header->data_length;
                        char content_buff[header->data_length];
                        read_size = readn(poll_fd_list[i].fd, &content_buff, header->data_length);
                        if (read_size < header->data_length) {
                            LERROR(UBusMaster) << "Failed to read content";
                            continue;
                        }
                        content = std::string(content_buff, header->data_length);
                        LINFO(UBusMaster) << "Content : " << content;
                    }

                    switch (header->message_type) {
                        case FRAME_INITIATION:
                            LERROR(UBusMaster) << "Invalid frame header";
                            break;
                        case FRAME_KEEP_ALIVE: {
                            LTRACE(UBusMaster) << "Keep alive message";
                            WritingSharedLockGuard shared_lock(participant_info_mtx_);
                            socket_participant_mapping_[poll_fd_list[i].fd]->watchdog_counter = 0;
                        } break;
                        case FRAME_EVENT_REGISTER:
                            LINFO(UBusMaster) << "New publish message";
                            {
                                std::string response;
                                nlohmann::json content_json = nlohmann::json::parse(content);
                                ReadingSharedLockGuard shared_lock(participant_info_mtx_);
                                std::lock_guard<std::mutex> lock_event(event_list_mtx_);
                                if (!content_json.contains("topic") || !content_json.contains("type_id")) {
                                    LDEBUG(UBusMaster) << "Invalid frame";
                                    response = "INVALID";
                                } else if (event_list_.find(content_json.at("topic")) != event_list_.end()) {
                                    response = "DUPLICATE";
                                } else {
                                    EventInfo event_info;
                                    event_info.name = content_json.at("topic");
                                    event_info.type = content_json.at("type_id");
                                    event_info.publisher = socket_participant_mapping_[poll_fd_list[i].fd];
                                    event_list_[content_json.at("topic")] = event_info;
                                    event_info.publisher->published_topic_list[event_info.name] = event_info.type;
                                    response = "OK";
                                }
                                Frame frame;
                                nlohmann::json response_json;
                                response_json["response"] = response;
                                std::string serialized_string = response_json.dump();
                                const char *char_struct = serialized_string.c_str();
                                frame.header.data_length = htonl(static_cast<uint32_t>(serialized_string.size()));
                                frame.data = new uint8_t[ntohl(frame.header.data_length)];
                                strncpy(reinterpret_cast<char *>(frame.data), char_struct, serialized_string.size());
                                int32_t ret;
                                if ((ret = writen(poll_fd_list[i].fd, static_cast<void *>(&frame.header),
                                                  sizeof(FrameHeader))) < 0) {
                                    LINFO(UBusMaster) << "Write returned " << ret;
                                }
                                if ((ret = writen(poll_fd_list[i].fd, static_cast<void *>(frame.data),
                                                  ntohl(frame.header.data_length))) < 0) {
                                    LINFO(UBusRuntime) << "Write returned " << ret;
                                }
                                delete[] frame.data;
                            }
                            break;
                        case FRAME_EVENT_SUBSCRIBE:
                            LINFO(UBusMaster) << "New subscribe message";
                            {
                                std::string response;
                                std::string publisher_ip;
                                int32_t publisher_port;
                                std::string publisher_name;
                                nlohmann::json content_json = nlohmann::json::parse(content);
                                if (!content_json.contains("topic") || !content_json.contains("type_id")) {
                                    LDEBUG(UBusMaster) << "Invalid frame";
                                    response = "INVALID";
                                } else {
                                    std::lock_guard<std::mutex> lock_event(event_list_mtx_);
                                    auto event_info = event_list_.find(content_json.at("topic"));
                                    if (event_info == event_list_.end()) {
                                        response = "NOT_PUBLISHED";
                                    } else {
                                        publisher_ip = event_info->second.publisher->listening_ip;
                                        publisher_port = event_info->second.publisher->listening_port;
                                        publisher_name = event_info->second.publisher->name;
                                        response = "OK";
                                    }
                                }
                                Frame frame;
                                nlohmann::json response_json;
                                response_json["response"] = response;
                                if (response == "OK") {
                                    response_json["publisher_ip"] = publisher_ip;
                                    response_json["publisher_port"] = publisher_port;
                                    response_json["publisher_name"] = publisher_name;
                                }

                                std::string serialized_string = response_json.dump();
                                const char *char_struct = serialized_string.c_str();
                                frame.header.data_length = htonl(static_cast<uint32_t>(serialized_string.size()));
                                frame.data = new uint8_t[ntohl(frame.header.data_length)];
                                strncpy(reinterpret_cast<char *>(frame.data), char_struct, serialized_string.size());
                                int32_t ret;
                                if ((ret = writen(poll_fd_list[i].fd, static_cast<void *>(&frame.header),
                                                  sizeof(FrameHeader))) < 0) {
                                    LDEBUG(UBusMaster) << "Write returned " << ret;
                                }
                                if ((ret = writen(poll_fd_list[i].fd, static_cast<void *>(frame.data),
                                                  ntohl(frame.header.data_length))) < 0) {
                                    LDEBUG(UBusRuntime) << "Write returned " << ret;
                                }
                                delete[] frame.data;
                            }
                            break;
                        case FRAME_METHOD_PROVIDE: {
                            std::string response;
                            nlohmann::json content_json = nlohmann::json::parse(content);
                            ReadingSharedLockGuard shared_lock(participant_info_mtx_);
                            std::lock_guard<std::mutex> lock_method(method_list_mtx_);
                            if (!content_json.contains("method") || !content_json.contains("request_type_id") ||
                                !content_json.contains("response_type_id")) {
                                LDEBUG(UBusMaster) << "Invalid frame";
                                response = "INVALID";
                            } else if (method_list_.find(content_json.at("method")) != method_list_.end()) {
                                response = "DUPLICATE";
                            } else {
                                MethodInfo method_info;
                                method_info.name = content_json.at("method");
                                method_info.request_type = content_json.at("request_type_id");
                                method_info.response_type = content_json.at("response_type_id");
                                method_info.provider = socket_participant_mapping_[poll_fd_list[i].fd];
                                method_list_[content_json.at("method")] = method_info;
                                method_info.provider->method_list[method_info.name] =
                                    std::make_pair(method_info.request_type, method_info.response_type);
                                response = "OK";
                            }
                            Frame frame;
                            nlohmann::json response_json;
                            response_json["response"] = response;
                            std::string serialized_string = response_json.dump();
                            const char *char_struct = serialized_string.c_str();
                            frame.header.data_length = htonl(static_cast<uint32_t>(serialized_string.size()));
                            frame.data = new uint8_t[ntohl(frame.header.data_length)];
                            strncpy(reinterpret_cast<char *>(frame.data), char_struct, serialized_string.size());
                            int32_t ret;
                            if ((ret = writen(poll_fd_list[i].fd, static_cast<void *>(&frame.header),
                                              sizeof(FrameHeader))) < 0) {
                                LINFO(UBusMaster) << "Write returned " << ret;
                            }
                            if ((ret = writen(poll_fd_list[i].fd, static_cast<void *>(frame.data),
                                              ntohl(frame.header.data_length))) < 0) {
                                LINFO(UBusMaster) << "Write returned " << ret;
                            }
                            delete[] frame.data;
                        } break;
                        case FRAME_METHOD_QUERY:
                            LINFO(UBusMaster) << "New method query message";
                            {
                                std::string response;
                                std::string provider_ip;
                                int32_t provider_port;
                                std::string provider_name;
                                nlohmann::json content_json = nlohmann::json::parse(content);
                                if (!content_json.contains("method") || !content_json.contains("request_type_id") ||
                                    !content_json.contains("response_type_id")) {
                                    LDEBUG(UBusMaster) << "Invalid frame";
                                    response = "INVALID";
                                } else {
                                    std::lock_guard<std::mutex> lock_method(method_list_mtx_);
                                    auto method_info = method_list_.find(content_json.at("method"));
                                    if (method_info == method_list_.end()) {
                                        response = "NOT_PUBLISHED";
                                    } else {
                                        provider_ip = method_info->second.provider->listening_ip;
                                        provider_port = method_info->second.provider->listening_port;
                                        provider_name = method_info->second.provider->name;
                                        response = "OK";
                                    }
                                }
                                Frame frame;
                                nlohmann::json response_json;
                                response_json["response"] = response;
                                if (response == "OK") {
                                    response_json["provider_ip"] = provider_ip;
                                    response_json["provider_port"] = provider_port;
                                    response_json["provider_name"] = provider_name;
                                }

                                std::string serialized_string = response_json.dump();
                                const char *char_struct = serialized_string.c_str();
                                frame.header.data_length = htonl(static_cast<uint32_t>(serialized_string.size()));
                                frame.data = new uint8_t[ntohl(frame.header.data_length)];
                                strncpy(reinterpret_cast<char *>(frame.data), char_struct, serialized_string.size());
                                int32_t ret;
                                if ((ret = writen(poll_fd_list[i].fd, static_cast<void *>(&frame.header),
                                                  sizeof(FrameHeader))) < 0) {
                                    LDEBUG(UBusMaster) << "Write returned " << ret;
                                }
                                if ((ret = writen(poll_fd_list[i].fd, static_cast<void *>(frame.data),
                                                  ntohl(frame.header.data_length))) < 0) {
                                    LDEBUG(UBusMaster) << "Write returned " << ret;
                                }
                                delete[] frame.data;
                            }
                            break;
                        case FRAME_DEBUG: {
                            std::string response;
                            process_debug_message(content, &response);
                            Frame frame;
                            frame.header.message_type = FRAME_DEBUG;
                            const char *char_struct = response.c_str();
                            frame.header.data_length = htonl(static_cast<uint32_t>(response.size()));
                            frame.data = new uint8_t[ntohl(frame.header.data_length)];
                            strncpy(reinterpret_cast<char *>(frame.data), char_struct, response.size());
                            int32_t ret;
                            if ((ret = writen(poll_fd_list[i].fd, static_cast<void *>(&frame.header),
                                              sizeof(FrameHeader))) < 0) {
                                LINFO(UBusMaster) << "Write returned " << ret;
                            }
                            if ((ret = writen(poll_fd_list[i].fd, static_cast<void *>(frame.data),
                                              ntohl(frame.header.data_length))) < 0) {
                                LINFO(UBusMaster) << "Write returned " << ret;
                            }
                            delete[] frame.data;
                        } break;
                        default:
                            break;
                    }
                }
            }
        }
        usleep(100000);
    }
}

void UBusMaster::accept_new_connection() {
    if (listen(control_sock_, 4096) < 0) {
        LDEBUG(UBusMaster) << "Failed to start listening";
        return;
    }

    sockaddr_in incoming_addr;
    bzero(&incoming_addr, sizeof(incoming_addr));
    uint32_t ret_size = sizeof(incoming_addr);
    int32_t fd = 0;
    while ((fd = accept(control_sock_, reinterpret_cast<sockaddr *>(&incoming_addr), &ret_size)) >= 0) {
        if (ret_size > sizeof(incoming_addr)) {
            LWARN(UBusMaster) << "Unexpected ret_size of accept()";
        }
        char header_buff[sizeof(FrameHeader)];
        size_t read_size = readn(fd, &header_buff, sizeof(FrameHeader));
        if (read_size < sizeof(FrameHeader)) {
            LERROR(UBusMaster) << "Failed to read header";
        } else {
            FrameHeader *header = reinterpret_cast<FrameHeader *>(header_buff);
            header->data_length = ntohl(header->data_length);
            if (header->message_type != FRAME_INITIATION) {
                LERROR(UBusMaster) << "Invalid frame header";
                continue;
            }
            LDEBUG(UBusMaster) << "Size of data to read : " << header->data_length;
            char content_buff[header->data_length];
            read_size = readn(fd, &content_buff, header->data_length);
            if (read_size < header->data_length) {
                LERROR(UBusMaster) << "Failed to read content";
            }
            std::string content(content_buff, header->data_length);
            LDEBUG(UBusMaster) << "Content : " << content;
            try {
                nlohmann::json json_struct = nlohmann::json::parse(content);
                std::string response;
                if (json_struct.contains("name") && json_struct.contains("listening_ip") &&
                    json_struct.contains("listening_port") && json_struct.contains("api_version")) {
                    std::lock_guard<std::mutex> lock(unprocessed_new_participants_mtx_);
                    WritingSharedLockGuard lock_shared(participant_info_mtx_);
                    if (json_struct["api_version"] != api_version_) {
                        response = "VERSION_MISMATCH";
                    } else if (participant_list_.find(json_struct["name"].get<std::string>()) ==
                               participant_list_.end()) {
                        std::shared_ptr<UBusParticipantInfo> participant_info = std::make_shared<UBusParticipantInfo>();
                        participant_info->name = json_struct["name"].get<std::string>();
                        participant_info->ip = std::string(inet_ntoa(incoming_addr.sin_addr));
                        participant_info->port = ntohs(incoming_addr.sin_port);
                        participant_info->listening_ip = json_struct["listening_ip"].get<std::string>();
                        participant_info->listening_port = json_struct["listening_port"].get<uint32_t>();
                        participant_info->socket = fd;
                        participant_list_[participant_info->name] = participant_info;
                        socket_participant_mapping_[fd] = participant_info;
                        LINFO(UBusMaster) << "Registered new participant :" << participant_info->name << "\n"
                                          << "Ip :" << participant_info->ip << "\n"
                                          << "Port :" << participant_info->port;
                        unprocessed_new_participants_.push(json_struct["name"].get<std::string>());
                        response = "OK";
                    } else {
                        LERROR(UBusMaster) << "Duplicate request for " << std::string(json_struct["name"]);
                        response = "DUPLICATE";
                    }
                } else {
                    LERROR(UBusMaster) << "Invalid joining request";
                    response = "INVALID";
                }
                Frame frame;
                nlohmann::json response_json;
                response_json["response"] = response;
                std::string serialized_string = response_json.dump();
                const char *char_struct = serialized_string.c_str();
                frame.header.data_length = htonl(static_cast<uint32_t>(serialized_string.size()));
                LDEBUG(UBusMaster) << "Data length : " << ntohl(frame.header.data_length);
                frame.data = new uint8_t[ntohl(frame.header.data_length)];
                strncpy(reinterpret_cast<char *>(frame.data), char_struct, serialized_string.size());
                int32_t ret;
                if ((ret = writen(fd, static_cast<void *>(&frame.header), sizeof(FrameHeader))) < 0) {
                    LINFO(UBusMaster) << "Write returned " << ret;
                }
                if ((ret = writen(fd, static_cast<void *>(frame.data), ntohl(frame.header.data_length))) < 0) {
                    LINFO(UBusRuntime) << "Write returned " << ret;
                }
                delete[] frame.data;
            } catch (nlohmann::json::exception &e) {
                LERROR(UBusMaster) << "Exception in json : " << e.what();
            }
        }
    }
}

void UBusMaster::keep_alive_worker() {
    while (1) {
        {
            std::lock_guard<std::mutex> lock(unprocessed_dead_participants_mtx_);
            ReadingSharedLockGuard lock_shared(participant_info_mtx_);
            for (auto &participant : participant_list_) {
                if (++participant.second->watchdog_counter >= 3) {
                    LINFO(UBusMaster) << participant.second->name << " is dead";
                    unprocessed_dead_participants_.push(participant.first);
                }
            }
        }
        sleep(1);
    }
}

void UBusMaster::process_debug_message(const std::string &input, std::string *output) {
    if (output == nullptr) {
        return;
    }

    nlohmann::json query_struct;
    try {
        query_struct = nlohmann::json::parse(input);
    } catch (nlohmann::json::exception &e) {
        LERROR(UBusDebugger) << "Exception in json : " << e.what();
    }

    if (!query_struct.contains("debug_type")) {
        return;
    }
    if (query_struct.at("debug_type") == "list_event") {
        nlohmann::json response_struct;
        response_struct["response"] = "OK";
        nlohmann::json event_list = nlohmann::json::array();
        std::lock_guard<std::mutex> lock(this->event_list_mtx_);
        for (auto &event : this->event_list_) {
            nlohmann::json event_struct;
            event_struct["name"] = event.second.name;
            event_struct["type"] = event.second.type;
            event_struct["publisher"] = event.second.publisher->name;
            event_list.push_back(event_struct);
        }
        response_struct["response_data"] = event_list;
        *output = response_struct.dump();
        return;
    }
    if (query_struct.at("debug_type") == "list_participant") {
        nlohmann::json response_struct;
        response_struct["response"] = "OK";
        nlohmann::json participant_list = nlohmann::json::array();
        ReadingSharedLockGuard lock_shared(this->participant_info_mtx_);
        for (auto &participant : this->participant_list_) {
            nlohmann::json participant_struct;
            participant_struct["name"] = participant.second->name;
            participant_struct["ip"] = participant.second->ip;
            participant_struct["port"] = participant.second->port;
            participant_struct["listening_ip"] = participant.second->listening_ip;
            participant_struct["listening_port"] = participant.second->listening_port;
            participant_list.push_back(participant_struct);
        }
        response_struct["response_data"] = participant_list;
        *output = response_struct.dump();
        return;
    }
    if (query_struct.at("debug_type") == "list_method") {
        nlohmann::json response_struct;
        response_struct["response"] = "OK";
        nlohmann::json method_list = nlohmann::json::array();
        std::lock_guard<std::mutex> lock(this->method_list_mtx_);
        for (auto &method : this->method_list_) {
            nlohmann::json method_struct;
            method_struct["name"] = method.second.name;
            method_struct["request_type"] = method.second.request_type;
            method_struct["response_type"] = method.second.response_type;
            method_struct["provider"] = method.second.provider->name;
            method_list.push_back(method_struct);
        }
        response_struct["response_data"] = method_list;
        *output = response_struct.dump();
        return;
    }

    return;
}