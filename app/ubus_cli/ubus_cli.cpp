/**
 * Wang Jiadong <jiadong.wang.94@outlook.com>
 */

#include "ubus_cli.hpp"

#include "nlohmann/json.hpp"

bool UBusDebugger::query_debug_info(const std::string &input, std::string *output) {
    if (output == nullptr) {
        return false;
    }

    int32_t sock = this->control_sock_;
    Frame frame;
    frame.header.message_type = FRAME_DEBUG;
    const char *char_struct = input.c_str();
    frame.header.data_length = htonl(static_cast<uint32_t>(input.size()));
    LDEBUG(UBusDebugger) << "Data length : " << ntohl(frame.header.data_length);
    frame.data = new uint8_t[ntohl(frame.header.data_length)];
    strncpy(reinterpret_cast<char *>(frame.data), char_struct, input.size());
    int32_t ret;
    if ((ret = writen(sock, static_cast<void *>(&frame.header), sizeof(FrameHeader))) < 0) {
        LDEBUG(UBusDebugger) << "Write returned " << ret;
    }
    LDEBUG(UBusDebugger) << "Sent header";
    if ((ret = writen(sock, static_cast<void *>(frame.data), ntohl(frame.header.data_length))) < 0) {
        LDEBUG(UBusDebugger) << "Write returned " << ret;
    }

    {
        char header_buff[sizeof(FrameHeader)];
        size_t read_size = readn(sock, &header_buff, sizeof(FrameHeader));
        if (read_size < sizeof(FrameHeader)) {
            LERROR(UBusDebugger) << "Failed to read header";
            return false;
        }
        FrameHeader *header = reinterpret_cast<FrameHeader *>(header_buff);
        header->data_length = ntohl(header->data_length);
        if (header->message_type != FRAME_DEBUG) {
            LERROR(UBusDebugger) << "Invalid frame type " << static_cast<int32_t>(header->message_type);
            return false;
        }
        char content_buff[header->data_length];
        read_size = readn(sock, &content_buff, header->data_length);
        if (read_size < header->data_length) {
            LERROR(UBusDebugger) << "Failed to read content";
        }
        std::string content(content_buff, header->data_length);
        *output = std::move(content);
        return true;
    }
}

bool UBusDebugger::query_event_list(std::string *out) {
    nlohmann::json query_struct;
    query_struct["debug_type"] = "list_event";
    std::string output;
    if (this->query_debug_info(query_struct.dump(), &output)) {
        nlohmann::json response_struct;
        try {
            response_struct = nlohmann::json::parse(output);
        } catch (nlohmann::json::exception &e) {
            LERROR(UBusDebugger) << "Exception in json : " << e.what();
            return false;
        }
        if (response_struct.contains("response") && response_struct.at("response") == "OK" &&
            response_struct.contains("response_data")) {
            nlohmann::json event_list;
            event_list = response_struct.at("response_data");
            if (out != nullptr) {
                *out = event_list.dump();
                return true;
            }
            if (event_list.is_array()) {
                for (auto &event : event_list) {
                    std::cout << "Event :" << std::endl;
                    std::cout << "    name      " << event.at("name").get<std::string>() << std::endl;
                    std::cout << "    type      " << event.at("type").get<uint32_t>() << std::endl;
                    std::cout << "    publisher " << event.at("publisher").get<std::string>() << std::endl;
                    std::cout << std::endl;
                }
            }
            return true;
        }
    } else {
        LERROR(UBusDebugger) << "Failed to query debug info from master";
        return false;
    }
    return false;
}

bool UBusDebugger::query_method_list(std::string *out) {
    nlohmann::json query_struct;
    query_struct["debug_type"] = "list_method";
    std::string output;
    if (this->query_debug_info(query_struct.dump(), &output)) {
        nlohmann::json response_struct;
        try {
            response_struct = nlohmann::json::parse(output);
        } catch (nlohmann::json::exception &e) {
            LERROR(UBusDebugger) << "Exception in json : " << e.what();
            return false;
        }
        if (response_struct.contains("response") && response_struct.at("response") == "OK" &&
            response_struct.contains("response_data")) {
            nlohmann::json method_list;
            method_list = response_struct.at("response_data");
            if (out != nullptr) {
                *out = method_list.dump();
                return true;
            }
            if (method_list.is_array()) {
                for (auto &method : method_list) {
                    std::cout << "Method :" << std::endl;
                    std::cout << "    name          " << method.at("name").get<std::string>() << std::endl;
                    std::cout << "    request_type  " << method.at("request_type").get<uint32_t>() << std::endl;
                    std::cout << "    response_type " << method.at("response_type").get<uint32_t>() << std::endl;
                    std::cout << "    provider      " << method.at("provider").get<std::string>() << std::endl;
                    std::cout << std::endl;
                }
            }
            return true;
        }
    } else {
        LERROR(UBusDebugger) << "Failed to query debug info from master";
        return false;
    }
    return false;
}

bool UBusDebugger::query_participant_list(std::string *out) {
    nlohmann::json query_struct;
    query_struct["debug_type"] = "list_participant";
    std::string output;
    if (this->query_debug_info(query_struct.dump(), &output)) {
        nlohmann::json response_struct;
        try {
            response_struct = nlohmann::json::parse(output);
        } catch (nlohmann::json::exception &e) {
            LERROR(UBusDebugger) << "Exception in json : " << e.what();
            return false;
        }
        if (response_struct.contains("response") && response_struct.at("response") == "OK" &&
            response_struct.contains("response_data")) {
            nlohmann::json participant_list;
            participant_list = response_struct.at("response_data");
            if (out != nullptr) {
                *out = participant_list.dump();
                return true;
            }
            if (participant_list.is_array()) {
                for (auto &participant : participant_list) {
                    if (participant.at("name").get<std::string>() == this->name_) {
                        continue;
                    }
                    std::cout << "Participant :" << std::endl;
                    std::cout << "    name           " << participant.at("name").get<std::string>() << std::endl;
                    std::cout << "    ip             " << participant.at("ip").get<std::string>() << std::endl;
                    std::cout << "    port           " << participant.at("port").get<uint32_t>() << std::endl;
                    std::cout << "    listening_ip   " << participant.at("listening_ip").get<std::string>()
                              << std::endl;
                    std::cout << "    listening_port " << participant.at("listening_port").get<uint32_t>() << std::endl;
                    std::cout << std::endl;
                }
            }
            return true;
        }

    } else {
        LERROR(UBusDebugger) << "Failed to query debug info from master";
        return false;
    }
    return false;
}

bool UBusDebugger::echo_event(const std::string &topic) {
    std::string event_list;
    if (!query_event_list(&event_list)) {
        return false;
    }
    nlohmann::json event_list_struct;
    try {
        event_list_struct = nlohmann::json::parse(event_list);
    } catch (nlohmann::json::exception &e) {
        LERROR(UBusDebugger) << "Exception in json : " << e.what();
        LERROR(UBusDebugger) << "Json data is " << event_list;
        return false;
    }
    int32_t type_id = 0;
    bool found = false;
    for (auto &element : event_list_struct) {
        if (element.contains("name") && element.at("name") == topic && element.contains("type")) {
            type_id = element.at("type").get<int32_t>();
            found = true;
        }
    }
    if (!found) {
        LERROR(UBusDebugger) << "Error, no such event";
        return false;
    }

    Frame frame;
    frame.header.message_type = FRAME_EVENT_SUBSCRIBE;

    nlohmann::json json_struct;
    json_struct["topic"] = topic;
    json_struct["type_id"] = type_id;
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
                        event_info.type = type_id;
                        event_info.socket = sub_socket;
                        event_info.publisher = response_json.at("publisher_name").get<std::string>();
                        event_info.callback =
                            std::make_shared<EventCallbackHolder<StringMsg> >([](const StringMsg &msg) {
                                std::cout << msg.data << std::endl;
                                std::cout << "---------" << std::endl;
                            });

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

bool UBusDebugger::request_method(const std::string &method_name,
                                  uint32_t request_type,
                                  const std::string &request,
                                  uint32_t response_type,
                                  std::string *response) {
    DebugMsg request_msg, response_msg;
    request_msg.id = request_type;
    request_msg.data = request;
    response_msg.id = response_type;
    if (this->call_method<DebugMsg, DebugMsg>(method_name, request_msg, &response_msg)) {
        *response = response_msg.data;
        return true;
    } else {
        return false;
    }
}