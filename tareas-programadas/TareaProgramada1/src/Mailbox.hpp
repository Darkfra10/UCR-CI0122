#ifndef MAILBOX_HPP
#define MAILBOX_HPP


/**
 * C++ class to encapsulate Unix message passing intrinsic structures and system calls
 * Francisco Ulate Alpízar C07901
*/
class MailBox {
    public:
        // Define the message structure within the class
        
        static const int MAX_SIZE = 1024;    // Maximum size of the message
        MailBox();
        ~MailBox();
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