#include "ubus_master.hpp"

#include "test.hpp"

int main() {
    InitFailureHandle();
    UBusMaster master;
    if (master.init("127.0.0.1", 5101)) {
        LOG(main) << "Init success" << std::endl;
    } else {
        LOG(main) << "Init failed" << std::endl;
    }
    master.run();
    return 0;
}