#include "ubus_master.hpp"

#include <poll.h>
#include <thread>

#include "nlohmann/json.hpp"

#include "helpers.hpp"
#include "frame.hpp"

bool UBusMaster::init(const std::string &ip, uint32_t port) {
    if ((control_sock_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOG(UBusMaster) << "Failed to create socket" << std::endl;
        control_sock_ = 0;
        return false;
    }
    sockaddr_in control_addr;
    bzero(&control_addr, sizeof(control_addr));
    control_addr.sin_family = AF_INET;
    control_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &control_addr.sin_addr) <= 0) {
        LOG(UBusMaster) << "Failed to convert ip address " << ip << std::endl;
        return false;
    }
    if (bind(control_sock_, reinterpret_cast<sockaddr *>(&control_addr),
             sizeof(control_addr)) < 0) {
        LOG(UBusMaster) << "Failed to convert bind to ip " << ip << " port "
                        << port << std::endl;
        return false;
    }

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
        while (!unprocessed_dead_participants_.empty()) {
            auto &participant = unprocessed_dead_participants_.front();
            unprocessed_dead_participants_.pop();
            for (int i = 0; i < max_connections_; ++i) {
                if (poll_fd_list[i].fd ==
                    participant_list_.at(participant)->socket) {
                    poll_fd_list[i].fd = -1;
                    break;
                }
            }
            auto ite = participant_list_.find(participant);
            if (ite != participant_list_.end()) {
                auto socket = ite->second->socket;
                participant_list_.erase(ite);
                auto ite2 = socket_participant_mapping_.find(socket);
                if (ite2 != socket_participant_mapping_.end()) {
                    socket_participant_mapping_.erase(ite2);
                }
            }
        }
        while (!unprocessed_new_participants_.empty()) {
            auto &participant = unprocessed_new_participants_.front();
            unprocessed_new_participants_.pop();
            for (int i = 0; i < max_connections_; ++i) {
                if (poll_fd_list[i].fd == -1) {
                    poll_fd_list[i].fd =
                        participant_list_.at(participant)->socket;
                    poll_fd_list[i].events = POLLIN;
                    poll_fd_list[i].revents = 0;
                    break;
                }
            }
        }
        int ret = poll(poll_fd_list, participant_list_.size(), 1000);
        if (ret < 0) {
            LOG(UBusMaster) << "Error in poll" << std::endl;
        } else if (ret == 0) {
            LOG(UBusMaster) << "Poll timeout" << std::endl;
        } else {
            for (int i = 0; i < max_connections_; ++i) {
                if (poll_fd_list[i].revents & POLLIN) {
                    LOG(UBusMaster) << "Socket " << poll_fd_list[i].fd
                                    << " is readable" << std::endl;
                    LOG(UBusMaster) << "Revents is " << poll_fd_list[i].revents
                                    << std::endl;
                    char header_buff[sizeof(FrameHeader)];
                    size_t read_size = read(poll_fd_list[i].fd, &header_buff,
                                            sizeof(FrameHeader));
                    if (read_size < sizeof(FrameHeader)) {
                        LOG(UBusMaster)
                            << "Failed to read header, size of data read : "
                            << read_size << std::endl;
                    } else {
                        FrameHeader *header =
                            reinterpret_cast<FrameHeader *>(header_buff);
                        switch (header->message_type) {
                            case FRAME_INITIATION:
                                LOG(UBusMaster)
                                    << "Invalid frame header" << std::endl;
                                break;
                            case FRAME_KEEP_ALIVE:
                                LOG(UBusMaster)
                                    << "Keep alive message" << std::endl;
                                socket_participant_mapping_[poll_fd_list[i].fd]
                                    ->watchdog_counter = 0;
                                break;
                            default:
                                break;
                        }
                        if (header->data_length > 0) {
                            LOG(UBusMaster) << "Size of data to read : "
                                            << header->data_length << std::endl;
                            char content_buff[header->data_length];
                            read_size = readn(poll_fd_list[i].fd, &content_buff,
                                              header->data_length);
                            if (read_size < header->data_length) {
                                LOG(UBusMaster)
                                    << "Failed to read content" << std::endl;
                            }
                            std::string content(content_buff,
                                                header->data_length);
                            LOG(UBusMaster)
                                << "Content : " << content << std::endl;
                        }
                    }
                }
            }
        }
        usleep(200000);
    }

    // int32_t epollfd = epoll_create1(0);
    // if (epollfd == -1) {
    //     LOG(UBusMaster) << "Failed to create epoll" << std::endl;
    //     return;
    // }
    // while (1) {
    //     if (participant_list_.size() == 0) {
    //         sleep(1);
    //         continue;
    //     }
    //     epoll_event events[participant_list_.size()];
    //     uint32_t index = 0;
    //     for (auto &participant : participant_list_) {
    //         events[index].events = EPOLLINl
    //         events[index].data.fd = participant.second->socket;
    //         index ++;
    //     }
    // };
}

void UBusMaster::accept_new_connection() {
    if (listen(control_sock_, 4096) < 0) {
        LOG(UBusMaster) << "Failed to start listening" << std::endl;
        return;
    }

    sockaddr_in incoming_addr;
    bzero(&incoming_addr, sizeof(incoming_addr));
    uint32_t ret_size;
    int32_t fd = 0;
    while ((fd = accept(control_sock_,
                        reinterpret_cast<sockaddr *>(&incoming_addr),
                        &ret_size)) >= 0) {
        char header_buff[sizeof(FrameHeader)];
        size_t read_size = readn(fd, &header_buff, sizeof(FrameHeader));
        if (read_size < sizeof(FrameHeader)) {
            LOG(UBusMaster) << "Failed to read header" << std::endl;
        } else {
            FrameHeader *header = reinterpret_cast<FrameHeader *>(header_buff);
            if (header->message_type != FRAME_INITIATION) {
                LOG(UBusMaster) << "Invalid frame header" << std::endl;
                continue;
            }
            LOG(UBusMaster) << "Size of data to read : " << header->data_length
                            << std::endl;
            char content_buff[header->data_length];
            read_size = readn(fd, &content_buff, header->data_length);
            if (read_size < header->data_length) {
                LOG(UBusMaster) << "Failed to read content" << std::endl;
            }
            std::string content(content_buff, header->data_length);
            LOG(UBusMaster) << "Content : " << content << std::endl;
            try {
                nlohmann::json json_struct = nlohmann::json::parse(content);
                if (json_struct.contains("name")) {
                    if (participant_list_.find(json_struct["name"]) ==
                        participant_list_.end()) {
                        std::shared_ptr<UBusParticipantInfo> participant_info =
                            std::make_shared<UBusParticipantInfo>();
                        participant_info->name = json_struct["name"];
                        participant_info->ip =
                            std::string(inet_ntoa(incoming_addr.sin_addr));
                        participant_info->port = ntohs(incoming_addr.sin_port);
                        participant_info->socket = fd;
                        participant_list_[participant_info->name] =
                            participant_info;
                        socket_participant_mapping_[fd] = participant_info;
                        LOG(UBusMaster)
                            << "Registered new participant :"
                            << participant_info->name << std::endl
                            << "Ip :" << participant_info->ip << std::endl
                            << "Port :" << participant_info->port << std::endl;
                        unprocessed_new_participants_.push(json_struct["name"]);

                        Frame frame;
                        nlohmann::json response_json;
                        response_json["response"] = "OK";
                        std::string serilized_string = response_json.dump();
                        const char *char_struct = serilized_string.c_str();
                        frame.header.data_length = serilized_string.size();
                        LOG(UBusMaster)
                            << "Data length : " << frame.header.data_length
                            << std::endl;
                        frame.data = new uint8_t[frame.header.data_length];
                        strncpy(reinterpret_cast<char *>(frame.data),
                                char_struct, serilized_string.size());
                        int32_t ret;
                        if ((ret = writen(participant_info->socket,
                                          static_cast<void *>(&frame.header),
                                          sizeof(FrameHeader))) < 0) {
                            LOG(UBusMaster)
                                << "Write returned " << ret << std::endl;
                        }
                        if ((ret = writen(participant_info->socket,
                                          static_cast<void *>(frame.data),
                                          frame.header.data_length)) < 0) {
                            LOG(UBusRuntime)
                                << "Write returned " << ret << std::endl;
                        }
                    } else {
                        LOG(UBusMaster) << "Duplicate request for "
                                        << json_struct["name"] << std::endl;
                        Frame frame;
                        nlohmann::json response_json;
                        response_json["response"] = "DUPLICATE";
                        std::string serilized_string = response_json.dump();
                        const char *char_struct = serilized_string.c_str();
                        frame.header.data_length = serilized_string.size();
                        LOG(UBusMaster)
                            << "Data length : " << frame.header.data_length
                            << std::endl;
                        frame.data = new uint8_t[frame.header.data_length];
                        strncpy(reinterpret_cast<char *>(frame.data),
                                char_struct, serilized_string.size());
                        int32_t ret;
                        if ((ret =
                                 writen(fd, static_cast<void *>(&frame.header),
                                        sizeof(FrameHeader))) < 0) {
                            LOG(UBusMaster)
                                << "Write returned " << ret << std::endl;
                        }
                        if ((ret = writen(fd, static_cast<void *>(frame.data),
                                          frame.header.data_length)) < 0) {
                            LOG(UBusRuntime)
                                << "Write returned " << ret << std::endl;
                        }
                    }
                } else {
                    LOG(UBusMaster) << "Invalid joining request" << std::endl;
                }
            } catch (nlohmann::json::exception &e) {
                LOG(UBusMaster)
                    << "Exception in json : " << e.what() << std::endl;
            }
        }
    }
}

void UBusMaster::keep_alive_worker() {
    while (1) {
        for (auto &participant : participant_list_) {
            if (++participant.second->watchdog_counter >= 6) {
                LOG(UBusMaster)
                    << participant.second->name << " is dead" << std::endl;
                unprocessed_dead_participants_.push(participant.first);
            }
        }
        sleep(1);
    }
}