#include "ubus_runtime.hpp"

#include "event.hpp"

#include <unistd.h>

int main() {
    UBusRuntime runtime;
    runtime.init("test_participant", "127.0.0.1", 5101);
    runtime.advertise_event<TestEvent1>("test_topic");
    runtime.advertise_event<TestEvent2>("test_topic2");
    sleep(10);
    runtime.publish_event("test_topic", TestEvent1());
    sleep(100);
    return 0;
}