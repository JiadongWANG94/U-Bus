#include "ubus_runtime.hpp"

#include "event.hpp"

#include <unistd.h>

#include "test.hpp"

int main() {
    InitFailureHandle();
    UBusRuntime runtime;
    runtime.init("test_participant_sub_2", "127.0.0.1", 5101);
    runtime.subscribe_event(
        "test_topic",
        std::function<void(const TestEvent1 &)>(
            [](const TestEvent1 &event) -> void {
                LOG(test_subscriber) << "Callback called" << std::endl;
                LOG(test_subscriber)
                    << "Received event data: " << event.data << std::endl;
            }));

    sleep(30);
    return 0;
}