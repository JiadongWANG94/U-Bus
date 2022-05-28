#include "ubus_runtime.hpp"

#include "event.hpp"

#include <unistd.h>

#include "test.hpp"

int main() {
    InitFailureHandle();
    UBusRuntime runtime;
    runtime.init("test_participant_requester", "127.0.0.1", 5101);
    TestEvent1 request;
    TestEvent2 response;
    sleep(10);
    runtime.call_method<TestEvent1, TestEvent2>("test_method", request, &response);
    sleep(10);
    return 0;
}