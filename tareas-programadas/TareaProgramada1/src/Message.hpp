#ifndef MESSAGE_HPP
#define MESSAGE_HPP
struct Message {
    long mtype;       // Message type, must be > 0
    char mtext[1024];  // Message data
};
#endif