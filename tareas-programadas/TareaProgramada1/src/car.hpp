#ifndef CAR_HPP
#define CAR_HPP
#include "Mailbox.hpp"

class Car {
    private:
        int id; // Car ID
        int queueNumber; // Is the street number where the car is waiting

    public:
        Car(int id, int queueNumber); // Constructor declaration
        ~Car(); // Destructor declaration
        int carWaitingTurn(Mailbox mailbox); // This method will be executed by the child process
        int allowPass(int numOfMessagesToSend, int messageType, Mailbox mailbox);
        int allowAllCarstoPass(std::unordered_map<long,long> allCar, Mailbox mailbox);
};





#endif