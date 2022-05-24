#include "ubus_runtime.hpp"

#include "event.hpp"

#include <unistd.h>

int main() {
    UBusRuntime runtime;
    runtime.init("test_participant_sub", "127.0.0.1", 5101);
    runtime.subscribe_event("test_topic",
                            std::function<void(const TestEvent1 &)>(
                                [](const TestEvent1 &event) -> void {
                                    LOG(test_subscriber)
                                        << " Callback called" << std::endl;
                                }));

    sleep(100);
    return 0;
}