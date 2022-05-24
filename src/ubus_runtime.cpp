#include "ubus_runtime.hpp"

#include <unistd.h>
#include <poll.h>
#include "nlohmann/json.hpp"

#include "helpers.hpp"
#include "frame.hpp"

bool UBusRuntime::init(const std::string &name,
                       const std::string &ip,
                       uint32_t port) {
    name_ = name;

    // init listening_sock
    if ((listening_sock_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOG(UBusMaster) << "Failed to create socket" << std::endl;
        listening_sock_ = 0;
        return false;
    }
    sockaddr_in listening_addr;
    bzero(&listening_addr, sizeof(listening_addr));
    listening_addr.sin_family = AF_INET;
    // let the system choose
    listening_addr.sin_port = htons(0);
    if (inet_pton(AF_INET, std::string("127.0.0.1").c_str(),
                  &listening_addr.sin_addr) <= 0) {
        LOG(UBusMaster) << "Failed to convert ip address " << ip << std::endl;
        return false;
    }
    if (bind(listening_sock_, reinterpret_cast<sockaddr *>(&listening_addr),
             sizeof(listening_addr)) < 0) {
        LOG(UBusMaster) << "Failed to convert bind to ip " << ip << " port "
                        << port << std::endl;
        return false;
    }
    uint32_t read_size;
    sockaddr_in socket_addr;
    bzero(&socket_addr, sizeof(socket_addr));
    read_size = sizeof(socket_addr);
    if (getsockname(listening_sock_, reinterpret_cast<sockaddr *>(&socket_addr),
                    &read_size) < 0) {
        LOG(UBusRuntime) << "Failed to getsockname" << std::endl;
        return false;
    }

    std::string listening_ip = inet_ntoa(socket_addr.sin_addr);
    int32_t listening_port = ntohs(socket_addr.sin_port);

    // init control_sock
    if ((control_sock_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOG(UBusRuntime) << "Failed to create socket" << std::endl;
        control_sock_ = 0;
        return false;
    }
    sockaddr_in control_addr;
    bzero(&control_addr, sizeof(control_addr));
    control_addr.sin_family = AF_INET;
    control_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &control_addr.sin_addr) <= 0) {
        LOG(UBusRuntime) << "Failed to convert ip address " << ip << std::endl;
        return false;
    }

    if (connect(control_sock_, reinterpret_cast<sockaddr *>(&control_addr),
                sizeof(control_addr)) != 0) {
        LOG(UBusRuntime) << "Failed to connect to master" << std::endl;
        return false;
    }

    Frame frame;
    frame.header.message_type = FRAME_INITIATION;
    nlohmann::json json_struct;
    json_struct["name"] = name;
    json_struct["listening_ip"] = listening_ip;
    json_struct["listening_port"] = listening_port;
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
                        LOG(UBusRuntime) << "Registered to master" << std::endl;
                    } else {
                        LOG(UBusRuntime)
                            << "Error from master : "
                            << response_json["response"] << std::endl;
                        return false;
                    }
                } else {
                    LOG(UBusRuntime)
                        << "Invalid response from master" << std::endl;
                }
            } catch (nlohmann::json::exception &e) {
                LOG(UBusRuntime)
                    << "Exception in json : " << e.what() << std::endl;
            }
        }
    }

    listening_worker_ = std::make_shared<std::thread>(
        &UBusRuntime::start_listening_socket, this);
    listening_worker_->detach();

    keep_alive_worker_ =
        std::make_shared<std::thread>(&UBusRuntime::keep_alive_sender, this);
    keep_alive_worker_->detach();

    event_worker_ = std::make_shared<std::thread>(
        &UBusRuntime::process_event_message, this);
    event_worker_->detach();
    return true;
}

void UBusRuntime::keep_alive_sender() {
    while (1) {
        Frame frame;
        frame.header.message_type = FRAME_KEEP_ALIVE;
        frame.header.data_length = 0;
        int32_t ret;
        if ((ret = writen(control_sock_, static_cast<void *>(&frame.header),
                          sizeof(FrameHeader))) < 0) {
            LOG(UBusRuntime) << "Write returned " << ret << std::endl;
        }
        // LOG(UBusRuntime) << "Keep Alive message sent" << std::endl;
        sleep(3);
    }
}

void UBusRuntime::start_listening_socket() {
    if (listen(listening_sock_, 4096) < 0) {
        LOG(UBusMaster) << "Failed to start listening" << std::endl;
        return;
    }

    sockaddr_in incoming_addr;
    bzero(&incoming_addr, sizeof(incoming_addr));
    uint32_t ret_size;
    int32_t fd = 0;
    while ((fd = accept(listening_sock_,
                        reinterpret_cast<sockaddr *>(&incoming_addr),
                        &ret_size)) >= 0) {
        char header_buff[sizeof(FrameHeader)];
        size_t read_size = readn(fd, &header_buff, sizeof(FrameHeader));
        if (read_size < sizeof(FrameHeader)) {
            LOG(UBusMaster) << "Failed to read header" << std::endl;
        } else {
            FrameHeader *header = reinterpret_cast<FrameHeader *>(header_buff);
            if (header->message_type != FRAME_SUBSCRIBE) {
                LOG(UBusMaster) << "Invalid frame header" << std::endl;
                continue;
            }
            char content_buff[header->data_length];
            read_size = readn(fd, &content_buff, header->data_length);
            if (read_size < header->data_length) {
                LOG(UBusMaster) << "Failed to read content" << std::endl;
            }
            std::string content(content_buff, header->data_length);
            std::string response;
            try {
                nlohmann::json response_json = nlohmann::json::parse(content);
                if (response_json.contains("topic") &&
                    response_json.contains("type_id") &&
                    response_json.contains("name")) {
                    // TODO: process subscription
                    LOG(UBusRuntime) << "New subscriber arrived "
                                     << response_json.at("name") << std::endl;
                    auto pub_event_info =
                        pub_list_.find(response_json.at("topic"));
                    if (pub_event_info == pub_list_.end()) {
                        LOG(UBusRuntime) << "Error wrong topic" << std::endl;
                        response = "INVALID";
                    } else if (pub_event_info->second.type !=
                               response_json.at("type_id").get<uint32_t>()) {
                        LOG(UBusRuntime) << "Error wrong type id" << std::endl;
                        response = "INVALID";
                    } else if (pub_event_info->second.client_socket_map.find(
                                   response_json.at("name")) !=
                               pub_event_info->second.client_socket_map.end()) {
                        LOG(UBusRuntime) << "Error duplicate" << std::endl;
                        response = "DUPLICATE";
                    } else {
                        LOG(UBusRuntime)
                            << "Registered new subscriber "
                            << response_json.at("name") << std::endl;
                        pub_event_info->second
                            .client_socket_map[response_json.at("name")] = fd;
                        response = "OK";
                    }

                } else {
                    LOG(UBusRuntime)
                        << "Invalid subscription request" << std::endl;
                    response = "INVALID";
                }
            } catch (nlohmann::json::exception &e) {
                LOG(UBusRuntime)
                    << "Exception in json : " << e.what() << std::endl;
                response = "INVALID";
            }
            {
                Frame frame;
                frame.header.message_type = FRAME_SUBSCRIBE;

                nlohmann::json json_struct;
                json_struct["response"] = response;
                std::string serilized_string = json_struct.dump();

                const char *char_struct = serilized_string.c_str();
                frame.header.data_length = serilized_string.size();
                frame.data = new uint8_t[frame.header.data_length];
                strncpy(reinterpret_cast<char *>(frame.data), char_struct,
                        serilized_string.size());
                int32_t ret;
                if ((ret = writen(fd, static_cast<void *>(&frame.header),
                                  sizeof(FrameHeader))) < 0) {
                    LOG(UBusRuntime) << "Write returned " << ret << std::endl;
                }
                if ((ret = writen(fd, static_cast<void *>(frame.data),
                                  frame.header.data_length)) < 0) {
                    LOG(UBusRuntime) << "Write returned " << ret << std::endl;
                }
            }
        }
    }
}

void UBusRuntime::process_event_message() {
    pollfd poll_fd_list[max_connections_];
    for (int i = 0; i < max_connections_; ++i) {
        poll_fd_list[i].fd = -1;
    }

    while (1) {
        while (!unprocessed_dead_sub_events_.empty()) {
            auto &sub_event = unprocessed_dead_sub_events_.front();
            unprocessed_dead_sub_events_.pop();
            for (int i = 0; i < max_connections_; ++i) {
                if (poll_fd_list[i].fd == sub_list_.at(sub_event).socket) {
                    poll_fd_list[i].fd = -1;
                    break;
                }
            }
            auto ite = sub_list_.find(sub_event);
            if (ite != sub_list_.end()) {
                auto socket = ite->second.socket;
                sub_list_.erase(ite);
            }
        }
        while (!unprocessed_new_sub_events_.empty()) {
            auto &sub_event = unprocessed_new_sub_events_.front();
            unprocessed_new_sub_events_.pop();
            for (int i = 0; i < max_connections_; ++i) {
                if (poll_fd_list[i].fd == -1) {
                    poll_fd_list[i].fd = sub_list_.at(sub_event).socket;
                    poll_fd_list[i].events = POLLIN;
                    poll_fd_list[i].revents = 0;
                    break;
                }
            }
        }
        int ret = poll(poll_fd_list, sub_list_.size(), 1000);
        if (ret < 0) {
            LOG(UBusRuntime) << "Error in poll" << std::endl;
        } else if (ret == 0) {
            // LOG(UBusRuntime) << "Poll timeout" << std::endl;
        } else {
            for (int i = 0; i < max_connections_; ++i) {
                if (poll_fd_list[i].revents & POLLIN) {
                    // LOG(UBusRuntime) << "Socket " << poll_fd_list[i].fd
                    //                 << " is readable" << std::endl;
                    // LOG(UBusRuntime) << "Revents is " <<
                    // poll_fd_list[i].revents
                    //                 << std::endl;
                    char header_buff[sizeof(FrameHeader)];
                    size_t read_size = read(poll_fd_list[i].fd, &header_buff,
                                            sizeof(FrameHeader));
                    if (read_size < sizeof(FrameHeader)) {
                        LOG(UBusRuntime)
                            << "Failed to read header, size of data read : "
                            << read_size << std::endl;
                    }
                    std::string content;
                    FrameHeader *header =
                        reinterpret_cast<FrameHeader *>(header_buff);
                    if (header->data_length > 0) {
                        LOG(UBusRuntime)
                            << "Size of data to read : " << header->data_length
                            << std::endl;
                        char content_buff[header->data_length];
                        read_size = readn(poll_fd_list[i].fd, &content_buff,
                                          header->data_length);
                        if (read_size < header->data_length) {
                            LOG(UBusRuntime)
                                << "Failed to read content" << std::endl;
                        }
                        content =
                            std::string(content_buff, header->data_length);
                        LOG(UBusRuntime)
                            << "Content : " << content << std::endl;
                    }

                    switch (header->message_type) {
                        case FRAME_EVENT:
                            LOG(UBusRuntime)
                                << "New event message" << std::endl;
                            {
                                // find the corresponding sub_event_info
                                for (auto &sub_event_info : sub_list_) {
                                    if (sub_event_info.second.socket ==
                                        poll_fd_list[i].fd) {
                                        LOG(UBusRuntime)
                                            << "Topic is "
                                            << sub_event_info.second.topic;
                                        (*sub_event_info.second.callback)(
                                            content);
                                        break;
                                    }
                                }
                            }
                            break;
                        default:
                            LOG(UBusRuntime)
                                << "Invalid frame header" << std::endl;
                            break;
                    }
                }
            }
        }
        usleep(100000);
    }
}