#include "ubus_master.hpp"

#include "test.hpp"

int main() {
    InitFailureHandle();
    g_log_manager.SetLogLevel(1);
    UBusMaster master;
    if (master.init("0.0.0.0", 5101)) {
        LINFO(main) << "Init success";
    } else {
        LINFO(main) << "Init failed";
    }
    master.run();
    return 0;
}