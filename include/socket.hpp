/**
 * Wang Jiadong <jiadong.wang.94@outlook.com>
 */

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

struct SocketAddress {
    std::string ip;
    uint32_t port;
};

class SocketSender {
 public:
    bool connect(const SocketAddress &);
    bool send(void *data, size_t length);
};

class SocketReceiver {
 public:
    bool receive(void **data, size_t *length);
};

class SocketListener {
 public:
    bool bind(const SocketAddress &);
    bool listen();
    std::shared_ptr<SocketSender> get_socket_sender(const SocketAddress &);
    std::shared_ptr<SocketReceiver> get_socket_receiver(const SocketAddress &);
};
