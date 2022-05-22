#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <memory>
#include <functional>
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
        uint32_t port;
        uint32_t socket;
        std::unordered_map<std::string, uint32_t> published_topic_list;
        std::unordered_map<std::string, uint32_t> subscribed_topic_list;
        std::unordered_map<std::string, uint32_t> method_list;
    };
    struct EventInfo {
        std::string name;
        uint32_t type;
        std::shared_ptr<UBusParticipantInfo> publisher;
        std::vector<std::shared_ptr<UBusParticipantInfo> > subscribers;
    };
    struct MethodInfo {
        std::string name;
        uint32_t type;
        std::shared_ptr<UBusParticipantInfo> provider;
    };
    std::unordered_map<std::string, std::shared_ptr<UBusParticipantInfo> >
        participant_list_;
    std::unordered_map<std::string, EventInfo> event_list_;
    std::unordered_map<std::string, MethodInfo> method_list_;

    int32_t control_sock_ = 0;

 private:
    const uint32_t keep_alive_interval_ = 1000;

 private:
    void check_participant_pulse();
    void process_control_message();
    void listening_control_message();
    void accept_new_connection();
};
