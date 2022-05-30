#include "ubus_runtime.hpp"

#include "test_message.hpp"
#include "message.hpp"

#include <unistd.h>
#include <sys/time.h>
#include <ctime>

#include "test.hpp"

void get_current_time(const NullMsg &in, TimestampMessage *out) {
    time_t currentTime;
    struct tm *localTime;

    time(&currentTime);
    localTime = localtime(&currentTime);

    out->h = localTime->tm_hour;
    out->m = localTime->tm_min;
    out->s = localTime->tm_sec;

    struct timeval time_now {};
    gettimeofday(&time_now, nullptr);
    out->ms = time_now.tv_usec / 1000 % 1000;
}

int main() {
    InitFailureHandle();
    UBusRuntime runtime;
    if (!runtime.init("test_clock_server", "127.0.0.1", 5101)) {
        return 1;
    }
    runtime.provide_method<NullMsg, TimestampMessage>(
        "test_method", std::function<void(const NullMsg &, TimestampMessage *)>(get_current_time));
    while (1) sleep(1000);
    return 0;
}