Reliable-UDP
============
This is a project to implement a reliable UDP.

Currently, the protocol used in reliable UDP is stop-and-wait (ARQ). In the future, Go-back-N will be used for efficiency.

The API of the reliable UDP protocol is in rdt_udp.h. The implementation is in rdt_udp.c.  The description of the reliable UDP is described in README.txt.

Also, the instruction for compiling is in README.txt.

The sample programs kft_server.c and kft_client.c are provided to illustrate how to use the reliable UDP protocol in file transfer between a server and a client.

In the implementation of the reliable UDP protocol, we use the function sendto_dropper (defined in dropper.c and dropper.h) to control the package loss rate for testing and performance measure. You can replace it by sendto function as usual.
