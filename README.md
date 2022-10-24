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
# to list published events
./ubus_cli list --event
# to list participants
./ubus_cli list --participant
# to subscribe an event and print in stdout
./ubus_cli echo --event <event topic>
```
