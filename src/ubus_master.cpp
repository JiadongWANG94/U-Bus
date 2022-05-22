#include "ubus_master.hpp"

// #include "unp.h"
#include <unistd.h>

#include "frame.hpp"

ssize_t /* Read "n" bytes from a descriptor. */
readn(int fd, void *vptr, size_t n) {
    size_t nleft;
    ssize_t nread;
    char *ptr;

    ptr = static_cast<char *>(vptr);
    nleft = n;
    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) < 0) {
            if (errno == EINTR)
                nread = 0; /* and call read() again */
            else
                return (-1);
        } else if (nread == 0)
            break; /* EOF */

        nleft -= nread;
        ptr += nread;
    }
    return (n - nleft); /* return >= 0 */
}
/* end readn */

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
    accept_new_connection();
    return true;
}

void UBusMaster::listening_control_message() {}

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
            char content_buff[header->data_length];
            read_size = readn(fd, &content_buff, header->data_length);
            if (read_size < header->data_length) {
                LOG(UBusMaster) << "Failed to read content" << std::endl;
            }
            std::string content(content_buff, header->data_length);
            LOG(UBusMaster) << "Content : " << content << std::endl;
        }
    }
}