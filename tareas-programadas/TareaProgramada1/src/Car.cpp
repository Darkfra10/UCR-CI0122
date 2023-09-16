#include "Car.hpp"
#include "Message.hpp"

#include <fcntl.h> /* For O_* constants */
#include <iostream>
#include <map>
#include <semaphore.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <locale.h>


// Rotonda de circunvalación
// A una rotonda de la carretera de circunvalación van a desembocar N calles de doble sentido. Cada carro que llega a la rotonda está representado por un proceso.
// Cada Carro(C) tiene el siguiente comportamiento cuando llega al final de la cola en la calle C:
// 1) Registra su llegada al final de la cola de la calle C, lo que indica que hay un carro más en la cola; además incrementa la cantidad de carros totales que esperan pasar
// 2) Determina si la cantidad de carros que hay en todas las colas (ntotal) sobrepasa K carros, donde K representa una constante
// 3) Si hay menos de K carros, espera a que se le de paso colocándose al final de la cola (C) de la calle correspondiente; sino determina cuál es la calle con mayor número de carros en espera (nmax carros en la cola qmax) y los deja pasar, las demás colas siguen en espera, en caso de que dos o más calles cumplan la condición de tener el máximo, escogerá la primera que encuentró
// 4) Si mientras están pasando los nmax carros de la cola elegida, llegan nuevos carros a esa cola, éstos deberán esperar hasta la siguiente vez que se permita el tránsito por esa calle
// 5) Cuando el carro ha ingresado a la rotonda, podremos considerar que finaliza
// 6) No existe un proceso controlador que administra la rotonda, solo existen los procesos
// representativos para cada carro que quiere circular por la rotonda
// Resuelva el problema utlizando procesos y paso de mensajes por medio de buzones

// Constructor
Car::Car(int id, int queueNumber) {
    this->id = id;
    this->queueNumber = queueNumber;
}

// Destructor
Car::~Car() {
    // Empty destructor, the car don't allocate any memory
}


/**
 * This method make the process wait until it is allowed to exit the roundabout. It waits for a message
 * from the mailbox with the same type as the  car->queueNumber.
 * @param mailbox Mailbox to receive the message
 * @return int Status of the message received
*/
int Car::carWaitingTurn(Mailbox& mailbox) {
    int st = -1;

    // Keep waiting until the car is allowed to exit
    Message carBehaviourMessage;
    carBehaviourMessage.mtype = this->queueNumber;

    mailbox.receive(&carBehaviourMessage);

    std::cout << "Car " << this->id << " is exiting street " << this->queueNumber << std::endl;
    std::cout << "Message received: " << carBehaviourMessage.mtext << std::endl;

    return st;
}


/**
 * This method will send multiple messages to the mailbox with the same type, this is to multiples cars
 * can exit the roundabout
 * @param numOfMessagesToSend Number of messages to send
 * @param messageType Type of message to send
 * @param mailbox Mailbox to send the message
 * @return int Status of the message sent
*/
int Car::allowPass(int numOfMessagesToSend, int messageType, Mailbox& mailbox) {
    int st = -1;

    // 1) First define create a Message object
    Message carBehaviourMessage;
    carBehaviourMessage.mtype = messageType;

    for (int i = 0; i < numOfMessagesToSend; i++) {
        std::string message = "Car number " + std::to_string(i) + " in the street " + std::to_string(messageType) + " is allowed to pass";
        std::copy(message.begin(), message.end(), carBehaviourMessage.mtext);

        // 2) Send the message
        int st = mailbox.send(&carBehaviourMessage);
        if (st == -1) {
            std::cerr << "Error sending message" << std::endl;
            std::cerr << "Error: " << strerror(errno) << std::endl;
            exit(1);
        }
    }

    return st;
}


void Car::allowAllCarsToPass(std::map<long,long> allCars, Mailbox& mailbox) {

    std::map<long, long>::iterator it = allCars.begin();

    while (it != allCars.end()) {
        // 1) First define create a Message object
        long messagetType = it->first;
        long totalMessagesToSend = it->second;

        // Send the message do a for loop until the totalMessagesToSend is reached
        for (int i = 0; i < totalMessagesToSend; i++) {
            Message carBehaviourMessage;
            carBehaviourMessage.mtype = messagetType;

            std::string message = "Car number " + std::to_string(i) + " in the street " + std::to_string(messagetType) + " is allowed to pass";
            std::copy(message.begin(), message.end(), carBehaviourMessage.mtext);

            // 2) Send the message
            int st = mailbox.send(&carBehaviourMessage);
            if (st == -1) {
                std::cerr << "Error sending message" << std::endl;
                std::cerr << "Error: " << strerror(errno) << std::endl;
                exit(1);
            }
            std::cout << "Message sent: " << carBehaviourMessage.mtext << std::endl;
        }
    }
}
