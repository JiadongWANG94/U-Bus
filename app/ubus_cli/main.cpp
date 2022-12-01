
#include "CLI11.hpp"

#include "ubus_cli.hpp"

int main(int argc, char **argv) {
    g_log_manager.SetLogLevel(4);
    CLI::App app{"ubus_cli: command line interface for monitoring, analysing and debugging ubus applications"};
    std::string master_ip = "127.0.0.1";
    app.add_option("--master_ip", master_ip, "ip of ubus master, default: 127.0.0.1");
    uint32_t master_port = 5101;
    app.add_option("--master_port", master_port, "port of ubus master, default: 5101");
    app.require_subcommand();
    CLI::App *subcom_list = app.add_subcommand("list", "list event, participant or method");
    bool list_event = false;
    bool list_participant = false;
    bool list_method = false;
    subcom_list->add_flag("--event", list_event, "list published event");
    subcom_list->add_flag("--participant", list_participant, "list participant");
    subcom_list->add_flag("--method", list_method, "list method");

    CLI::App *subcom_echo = app.add_subcommand("echo", "echo message of specific event");
    std::string echo_event = "";
    subcom_echo->add_option("--event", echo_event, "event to echo")->required();

    CLI::App *subcom_dump = app.add_subcommand("dump", "dump event messages");

    CLI::App *subcom_request = app.add_subcommand("request", "request method");
    std::string request_method;
    uint32_t request_type = 0;
    uint32_t response_type = 0;
    std::string request_body;
    subcom_request->add_option("--method", request_method, "name of method")->required();
    subcom_request->add_option("--request_type", request_type, "type of request data")->required();
    subcom_request->add_option("--response_type", response_type, "type of response data")->required();
    subcom_request->add_option("--request_body", request_body, "serialized request content")->required();

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        return app.exit(e);
    }

    if (subcom_list->parsed()) {
        if (!list_event && !list_participant && !list_method) {
        }

        if (list_event) {
            UBusDebugger debugger;
            debugger.init("debugger" + std::to_string(getpid()), master_ip, master_port);
            debugger.query_event_list();
        }
        if (list_participant) {
            UBusDebugger debugger;
            debugger.init("debugger" + std::to_string(getpid()), master_ip, master_port);
            debugger.query_participant_list();
        }
        if (list_method) {
            UBusDebugger debugger;
            debugger.init("debugger" + std::to_string(getpid()), master_ip, master_port);
            debugger.query_method_list();
        }
    }

    if (subcom_echo->parsed()) {
        UBusDebugger debugger;
        debugger.init("debugger" + std::to_string(getpid()), master_ip, master_port);
        debugger.echo_event(echo_event);
        while (true) {
            sleep(1);
        }
    }

    if (subcom_request->parsed()) {
        UBusDebugger debugger;
        debugger.init("debugger" + std::to_string(getpid()), master_ip, master_port);
        std::string response_body;
        debugger.request_method(request_method, request_type, request_body, response_type, &response_body);
        std::cout << response_body << std::endl;
    }
    return 0;
}