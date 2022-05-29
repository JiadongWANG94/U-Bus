#include "ubus_runtime.hpp"

#include "test_message.hpp"

#include <unistd.h>

#include "test.hpp"

int main() {
    InitFailureHandle();
    UBusRuntime runtime;
    runtime.init("test_participant_requester", "127.0.0.1", 5101);
    TestMessage1 request;
    TestMessage2 response;
    sleep(10);
    runtime.call_method<TestMessage1, TestMessage2>("test_method", request, &response);
    sleep(10);
    return 0;
}