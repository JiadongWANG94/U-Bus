#include "ubus_runtime.hpp"

#include "test_message.hpp"
#include "message.hpp"

#include <unistd.h>

#include "test.hpp"

int main() {
    InitFailureHandle();
    UBusRuntime runtime;
    runtime.init("test_clock_client", "127.0.0.1", 5101);
    TimestampMessage response;
    runtime.call_method<NullMsg, TimestampMessage>("test_method", NullMsg(), &response);
    LINFO(main) << "Current timestamp is " << response.h << ":" << response.m << ":" << response.s << "." << response.ms
                << std::endl;
    return 0;
}