#include "Mailbox.hpp"
#include "Message.hpp"

#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

Mailbox::Mailbox() {
    // Use msgget to create a new mailbox
    // IPC_CREAT: Create the mailbox if it doesn't exist
    // IPC_EXCL: Fail if the mailbox already exists
    // ACCESS: Allow all to read and write to the mailbox
    this->mailbox_id = msgget(this->KEY, IPC_CREAT | this->ACCESS);
    if (this->mailbox_id == -1) {
        std::cerr << "Error creating mailbox" << std::endl;
        std::cerr << "Error: " << strerror(errno) << std::endl;
        exit(1);
    }
    std::cout << "Mailbox created with id: " << this->mailbox_id << std::endl;
}

Mailbox::~Mailbox() {
    // Use msgctl to destroy mailbox
    // IPC_RMID: Remove the mailbox
    // NULL: No flags
    int st = -1;
    std::cout << "Destroying mailbox with id: " << this->mailbox_id << std::endl;
    st = msgctl(this->mailbox_id, IPC_RMID, NULL);
    if (st == -1) {
        std::cerr << "Error destroying mailbox" << std::endl;
        std::cerr << "Error: " << strerror(errno) << std::endl;
        exit(1);
    }
}

int Mailbox::getNumPendingMessages() {
    // int msgctl(int msqid, int cmd, struct msqid_ds *buf);
    // struct msqid_ds {
    //     struct ipc_perm msg_perm;   /* Ownership and permissions */
    //     time_t          msg_stime;  /* Time of last msgsnd(2) */
    //     time_t          msg_rtime;  /* Time of last msgrcv(2) */
    //     time_t          msg_ctime;  /* Time of creation or last
    //                                     modification by msgctl() */
    //     unsigned long   msg_cbytes; /* # of bytes in queue */
    //     msgqnum_t       msg_qnum;   /* # number of messages in queue */
    //     msglen_t        msg_qbytes; /* Maximum # of bytes in queue */
    //     pid_t           msg_lspid;  /* PID of last msgsnd(2) */
    //     pid_t           msg_lrpid;  /* PID of last msgrcv(2) */
    // };
    struct msqid_ds messageInfo;

    if (msgctl(this->mailbox_id, IPC_STAT, &messageInfo) == -1) {
        std::cerr << "Error getting message queue information: " << strerror(errno) << std::endl;
        return -1; // Return -1 to indicate an error
    }

    return messageInfo.msg_qnum;
}

void Mailbox::deleteAllMessages() {
    // Delete all messages
    int st = -1;
    st = msgctl(this->mailbox_id, IPC_RMID, NULL);
    if (st == -1) {
        std::cerr << "Error destroying mailbox" << std::endl;
        std::cerr << "Error: " << strerror(errno) << std::endl;
        exit(1);
    }
}


/**
 * Send method
 * @param message Pointer to message structure to send it has to be a struct msgbuf with a type and a text
*/
int Mailbox::send(const void *message) {
    int st = -1;

    // The msgsnd() function is used to send a message to the queue associated with the message queue identifier specified by msgid.
    // The argument msgp points to a user-defined buffer that must contain first a field of type long int that will specify the type of the message, and then a data portion that will hold the data bytes of the message

    // Get the idea from https://stackoverflow.com/questions/27995692/reinterpret-cast-casts-away-qualifiers
    Message* message_buffer = const_cast<Message*>(reinterpret_cast<const Message*>(message));

    // validate the type of the message
    if (message_buffer->mtype < 0) {
        std::cerr << "Message type must be greater than 0" << std::endl;
        exit(1);
    }

    // validate the size of the message
    if (strnlen(message_buffer->mtext, MAX_SIZE) == MAX_SIZE) {
        std::cerr << "Message too long" << std::endl;
        exit(1);
    }

    st = msgsnd(this->mailbox_id, message_buffer, sizeof(message_buffer->mtext), 0);

    if (st == -1) {
        std::cerr << "Mailbox::send Error sending message" << std::endl;
        exit(1);
    }

    return st;
}


/**
 * Receive method
 * 
 * @param message Pointer to message struct to receive
 * @return int Number of bytes received
*/
int Mailbox::receive(void *message) {
    int st = -1;

    // msgbuf* message_buffer = reinterpret_cast<msgbuf*>(message);
    Message* message_buffer = reinterpret_cast<Message*>(message);
    if (message_buffer == nullptr) {
        std::cerr << "Message buffer is null" << std::endl;
        exit(1);
    }

    if (message_buffer->mtype < 0) {
        std::cerr << "Message type must be greater than 0" << std::endl;
        exit(1);
    }

    if (sizeof(message_buffer->mtext) > MAX_SIZE) {
        std::cerr << "Message too long" << std::endl;
        exit(1);
    }

    st = msgrcv(this->mailbox_id, reinterpret_cast<Message*>(message), sizeof(message_buffer->mtext), message_buffer->mtype, 0);

    if (st == -1) {
        std::cerr << "Error receiving message" << std::endl;
        std::cerr << "Error: " << strerror(errno) << std::endl;
        exit(1);
    }

    return st;
}

 // The msgrcv() system call removes a message from the queue specified by msqid and places it in the buffer pointed to by msgp.
// The argument msgsz specifies the maximum size in bytes for the member mtext of the structure pointed to by the msgp argument. 
// If the message text has length greater than msgsz, then the behavior depends on whether MSG_NOERROR is specified in msgflg. 
// If MSG_NOERROR is specified, then the message text will be truncated (and the truncated part will be lost); 
// if MSG_NOERROR is not specified, then the message isn't removed from the queue and the system call fails returning -1 with errno set to E2BIG.
// ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg);