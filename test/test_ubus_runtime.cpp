#include "ubus_runtime.hpp"

#include <unistd.h>

int main() {
    UBusRuntime runtime;
    runtime.init("test_participant", "127.0.0.1", 5100);

    sleep(100);
    return 0;
}