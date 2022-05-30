#include "ubus_runtime.hpp"

#include "test_message.hpp"
#include "message.hpp"

#include <unistd.h>
#include <sys/time.h>
#include <ctime>

#include "test.hpp"

void print_current_time() {
    time_t currentTime;
    struct tm *localTime;

    time(&currentTime);
    localTime = localtime(&currentTime);

    struct timeval time_now {};
    gettimeofday(&time_now, nullptr);

    LINFO(main) << "Local: Current timestamp is " << localTime->tm_hour << ":" << localTime->tm_min << ":"
                << localTime->tm_sec << "." << time_now.tv_usec / 1000 % 1000 << std::endl;
}

int main() {
    InitFailureHandle();
    UBusRuntime runtime;
    if (!runtime.init("test_clock_client", "127.0.0.1", 5101)) {
        return 1;
    }
    TimestampMessage response;
    runtime.call_method<NullMsg, TimestampMessage>("test_method", NullMsg(), &response);
    print_current_time();
    LINFO(main) << "Remote: Current timestamp is " << response.h << ":" << response.m << ":" << response.s << "."
                << response.ms << std::endl;
    return 0;
}