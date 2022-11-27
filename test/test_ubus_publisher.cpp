#include "ubus_runtime.hpp"

#include "test_message.hpp"

#include <unistd.h>

#include "test.hpp"

int main() {
    InitFailureHandle();
    g_log_manager.SetLogLevel(0);
    UBusRuntime runtime;
    runtime.init("test_participant", "127.0.0.1", 5101);
    runtime.advertise_event<TestMessage1>("test_topic");
    runtime.advertise_event<TestMessage2>("test_topic2");
    sleep(10);
    TestMessage1 event1;
    event1.data = "First message";
    runtime.publish_event("test_topic", event1);
    sleep(10);
    TestMessage1 event2;
    event2.data = "Second message";
    runtime.publish_event("test_topic", event2);
    sleep(10);
    while (1) {
        TestMessage1 event3;
        event3.data = "Loop message";
        runtime.publish_event("test_topic", event3);
        sleep(10);
    }

    return 0;
}