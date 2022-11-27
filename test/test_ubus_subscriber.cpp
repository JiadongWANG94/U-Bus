#include "ubus_runtime.hpp"

#include "test_message.hpp"

#include <unistd.h>

#include "test.hpp"

int main() {
    InitFailureHandle();
    g_log_manager.SetLogLevel(0);
    UBusRuntime runtime;
    runtime.init("test_participant_sub", "127.0.0.1", 5101);
    runtime.subscribe_event("test_topic",
                            std::function<void(const TestMessage1 &)>([](const TestMessage1 &event) -> void {
                                LINFO(test_subscriber) << "Callback called";
                                LINFO(test_subscriber) << "Received event data: " << event.data;
                            }));

    sleep(30);
    return 0;
}