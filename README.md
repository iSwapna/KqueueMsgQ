# KqueueMsgQ
A Scalable Message Queue Server using FreeBSD Kqueue notification in C++

The basic message queue listens on a socket and accepts connections from producers and consumers. Producers may push messages into the queue, and consumers subscribe to some number of queue names and receive messages as they become available. No persistence and no delivery guarantees are provided.

* Single-threaded
* Consumers may subscribe to multiple queues
* Supports several concurrent producers and consumers

## Protocol

When a client connects, it identifies as either a producer or a consumer. This is done be sending one of two strings `produce` or `consume` which both occupy 7 bytes.

The protocol has three event types:
- `*` representing a message either being created or delivered to a worker. Producers may send this message, and consumers will receive these.
- `>` which is a client requesting to receive messages from a named queue. Only a consumer may send this message type.
- `/` which requests to *stop* receiving messages from a named queue. Only a consumer may send this message type.

### Message event
- **Byte 1**: `*`
- **Next 4 bytes**: Length of queue name; referenced as *name_len* in remainder of spec. This is an unsigned 32-bit integer in network byte order.
- **Next *name_len* bytes**: The queue name; arbitrary byte sequence.
- **Next 4 bytes**: Length of message; referenced as *msg_len* in remainder of spec. This is an unsigned 32-bit integer in network byte order.
- **Next *msg_len* bytes**: The message being sent; arbitrary byte sequence.

For example, this message could be visualized (ignoring integer encoding) as
`*5queue12Hello, world`

### Subscribe event
When a worker wants to subscribe to receiving messages for some queue.

- **Byte 1**: `>`
- **Next 4 bytes**: Length of queue name; referenced as *name_len* in remainder of spec. This is an unsigned 32-bit integer in network byte order.
- **Next *name_len* bytes**: The queue name; arbitrary byte sequence.

For example, this message could be visualized (ignoring integer encoding) as
`>5queue`
And would have the effect of subscribing to the "queue" queue. Note that the message `>0` should subscribe to all queues (no queue is named).

### Unsubscribe event
When a worker wants to unsusbscribe from receiving events from a queue.

- **Byte 1**: `/`
- **Next 4 bytes**: Length of queue name; referenced as *name_len* in remainder of spec. This is an unsigned 32-bit integer in network byte order.
- **Next *name_len* bytes**: The queue name; arbitrary byte sequence.

For example, this message could be visualized (ignoring integer encoding) as
`/5queue`
And would have the effect of unsubscribing from the "queue" queue. Note that `/0` is a special case which unsubscribes from all queues.

### Compile and Install
Tested on Clang 802.0, whith C++11 libraries

`g++ kq.cpp -std=c++11 -o Server.o`
`g++ Client.cpp -std=c++11 -o Client.o`

### Testing
Start Server with port no and then start multiple clients. To test on localhost:

`./Server.o 8000`

`./Client.o 127.0.0.1 8000`



