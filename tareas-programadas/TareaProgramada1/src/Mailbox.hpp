#ifndef MAILBOX_HPP
#define MAILBOX_HPP
#define MAX_SIZE 1024


/**
 * C++ class to encapsulate Unix message passing intrinsic structures and system calls
 * Francisco Ulate Alpízar C07901
*/
class Mailbox {
    public:
        // Define the message structure within the class

        Mailbox();
        ~Mailbox();
        int send(const void *message); // Message is the structure to send
        int receive(void *message);	// Message is the structure to receive
        int getNumPendingMessages(); // Get the number of pending messages
        int getNumPendingMessagesForType(int type); // Get the number of pending messages for a given type

    private:
        const int KEY = 1000;	// Key value for this mailbox
        const int ACCESS = 0666; // Allow all to read and write to the mailbox
        int mailbox_id;		// ID returned by msgget() call

        struct msgbuf {
            long mtype;       // Message type, must be > 0
            char mtext[MAX_SIZE];  // Message data
        };
};



#endif // MAILBOX_HPP