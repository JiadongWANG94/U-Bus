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

#include <string>
#include <memory>
#include <queue>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include "nlohmann/json.hpp"

#include "log.hpp"

class UBusMaster {
 public:
    bool init(const std::string &ip, uint32_t port);
    bool run();

 private:
    struct UBusParticipantInfo {
        std::string name;
        std::string ip;
        uint32_t port = 0;
        uint32_t socket = 0;
        std::string listening_ip;
        uint32_t listening_port = 0;
        std::unordered_map<std::string, uint32_t> published_topic_list;
        std::unordered_map<std::string, uint32_t> subscribed_topic_list;
        std::unordered_map<std::string, std::pair<uint32_t, uint32_t> > method_list;
        uint8_t watchdog_counter = 0;
    };
    struct EventInfo {
        std::string name;
        uint32_t type;
        std::shared_ptr<UBusParticipantInfo> publisher;
        std::vector<std::shared_ptr<UBusParticipantInfo> > subscribers;
    };
    struct MethodInfo {
        std::string name;
        uint32_t request_type;
        uint32_t response_type;
        std::shared_ptr<UBusParticipantInfo> provider;
    };
    std::unordered_map<std::string, std::shared_ptr<UBusParticipantInfo> > participant_list_;
    std::unordered_map<uint32_t, std::shared_ptr<UBusParticipantInfo> > socket_participant_mapping_;
    std::shared_mutex participant_info_mtx_;

    std::queue<std::string> unprocessed_new_participants_;
    std::mutex unprocessed_new_participants_mtx_;
    std::queue<std::string> unprocessed_dead_participants_;
    std::mutex unprocessed_dead_participants_mtx_;

    std::unordered_map<std::string, EventInfo> event_list_;
    std::mutex event_list_mtx_;

    std::unordered_map<std::string, MethodInfo> method_list_;
    std::mutex method_list_mtx_;

    int32_t control_sock_ = 0;

    int32_t max_connections_ = 1024;

 private:
    const uint32_t keep_alive_interval_ = 1000;

 private:
    void check_participant_pulse();
    void process_control_message();
    void listening_control_message();
    void accept_new_connection();
    void keep_alive_worker();
};
