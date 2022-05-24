#include "ubus_runtime.hpp"

#include "event.hpp"

#include <unistd.h>

#include "test.hpp"

int main() {
    InitFailureHandle();
    UBusRuntime runtime;
    runtime.init("test_participant", "127.0.0.1", 5101);
    runtime.advertise_event<TestEvent1>("test_topic");
    runtime.advertise_event<TestEvent2>("test_topic2");
    sleep(10);
    TestEvent1 event1;
    event1.data = "First message";
    runtime.publish_event("test_topic", event1);
    sleep(10);
    TestEvent1 event2;
    event2.data = "Second message";
    runtime.publish_event("test_topic", event2);
    sleep(10);
    return 0;
}