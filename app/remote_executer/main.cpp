/**
 * Wang Jiadong <jiadong.wang.94@outlook.com>
 */

#include "ubus_runtime.hpp"

#include <unistd.h>
#include <stdio.h>

#include "log.hpp"
#include "test.hpp"

#define MAX_RETURN_SIZE 4096

/*
    WARN:
    this sample app has security risk. don't run this in public network
*/

void ExecuteCommand(const StringMsg &req, StringMsg *rep) {
    LINFO(ExecuteCommand) << "Received command " << req.data;
    std::string bounded_cmd = "timeout 3 " + req.data;
    FILE *pipe = popen(bounded_cmd.c_str(), "r");
    if (pipe == nullptr) {
        LERROR(ExecuteCommand) << "Fail to open pipe";
    }
    std::string popen_output = "";
    char buff[1024];
    LDEBUG(ExecuteCommand) << "Read results from pipe..";
    while (fgets(buff, sizeof(buff), pipe) && popen_output.size() < MAX_RETURN_SIZE) {
        popen_output += buff;
    }
    auto ret = pclose(pipe);
    if (ret != 0) {
        LERROR(ExecuteCommand) << "Fail to execute pclose";
    }
    rep->data = std::move(popen_output);
}

int main() {
    InitFailureHandle();
    g_log_manager.SetLogLevel(2);
    UBusRuntime runtime;
    runtime.init("remote_executer", "127.0.0.1", 5101);
    runtime.provide_method<StringMsg, StringMsg>("remote_execution", ExecuteCommand);
    while (1) sleep(100);
    return 0;
}