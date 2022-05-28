#include "ubus_runtime.hpp"

#include "event.hpp"

#include <unistd.h>

#include "test.hpp"

int main() {
    InitFailureHandle();
    UBusRuntime runtime;
    runtime.init("test_participant_provider", "127.0.0.1", 5101);
    runtime.provide_method<TestEvent1, TestEvent2>(
        "test_method",
        std::function<void(const TestEvent1 &, TestEvent2 *)>(
            [](const TestEvent1 &req, TestEvent2 *rep) -> void { LINFO(main) << "Method called" << std::endl; }));
    sleep(100);
    return 0;
}