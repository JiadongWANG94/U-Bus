#include "ubus_runtime.hpp"

#include "test_message.hpp"

#include <unistd.h>

#include "test.hpp"

int main() {
    InitFailureHandle();
    UBusRuntime runtime;
    runtime.init("test_participant_provider", "127.0.0.1", 5101);
    runtime.provide_method<TestMessage1, TestMessage2>(
        "test_method", std::function<void(const TestMessage1 &, TestMessage2 *)>(
                           [](const TestMessage1 &req, TestMessage2 *rep) -> void { LINFO(main) << "Method called"; }));
    sleep(100);
    return 0;
}