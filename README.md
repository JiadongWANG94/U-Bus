# U-Bus

A Service-Orianted Communication Middleware that supports the following patterns:
* Publisher/Subscriber (Event)
* Request/Reply (Method)

Features:
* Centralized: a master node needs to be run to ordinate. (we will support distributed architecture in the future)
* Socket-bsed: messages (include control messages and customized messages) are transmissed with socket. (we will support shared_memory in the future)

## Build

```
mkdir build && cd build
cmake ..
make
```

## Usage
* ubus-master needs to be launch before you start client applications
```
<path/to>/ubus-master
```
* Run your ubus client (or you can use the samples in `test` folder)
* Use `ubus-cli` for debug
```sh
$ ./ubus_cli --help
ubus_cli: command line interface for monitoring, analysing and debugging ubus applications
Usage: ./ubus_cli [OPTIONS] SUBCOMMAND

Options:
  -h,--help                   Print this help message and exit
  --master_ip TEXT            ip of ubus master, default: 127.0.0.1
  --master_port UINT          port of ubus master, default: 5101

Subcommands:
  list                        list event, participant or method
  echo                        echo message of specific event
  dump                        dump event messages
  request                     request method

# examples:

# to list published events
$ ./ubus_cli list --event
Event :
    name      test_topic2
    type      12
    publisher test_participant

Event :
    name      test_topic
    type      11
    publisher test_participant

# to list participants
$ ./ubus_cli list --participant
Participant :
    name           test_participant
    ip             127.0.0.1
    port           52078
    listening_ip   0.0.0.0
    listening_port 57513

# to subscribe an event and print in stdout
$ ./ubus_cli echo --event <event topic>
msg 1
---------
msg 2
---------
msg 3
---------
...

# to list methods
$ ./ubus_cli list --method
Method :
    name          time
    request_type  1
    response_type 13
    provider      test_clock_server

# to request a method (supposing you know the serielized request datatype)
$ ./ubus_cli request --method time --request_type 1 --response_type 13 --request_body ""
{"h":22,"m":37,"ms":787,"s":15}
```
