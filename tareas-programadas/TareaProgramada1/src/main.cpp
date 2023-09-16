#include "Car.hpp"
#include "Mailbox.hpp"
#include "Message.hpp"

#include <iostream>
#include <fcntl.h>           /* For O_* constants */
#include <semaphore.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <locale.h>

int main(int argc, char const *argv[]) {
    const key_t SHM_KEY = 1234;
    const long TOTAL_STREETS = 7;
    const long TOTAL_CARS = 2;
    const long MAX_CARS = 1;
    const size_t SHM_SIZE = sizeof(long) * TOTAL_STREETS * 2; // Multiply by 2 to have more space if needed

    Mailbox mailbox;
    if (mailbox.getNumPendingMessages() > 0) {
        // delete all messages
        mailbox.deleteAllMessages();
        exit(1);
    }


    // Share this memory between processes
    int shmid = shmget(SHM_KEY, 0, 0); // 0, 0 to get the size of the shared memory segment
    if (shmid != -1) {
        // Shared memory segment already exists, so we need to detach and delete it
        if (shmctl(shmid, IPC_RMID, NULL) == -1) {
            std::cerr << "Error deleting existing shared memory segment: " << strerror(errno) << std::endl;
            exit(1);
        }
    }


    

    // Define the shared memory segment here
    shmid = shmget(SHM_KEY, SHM_SIZE, IPC_CREAT | IPC_EXCL | 0666); 
    if (shmid == -1) {
        std::cerr << "Error creating shared memory segment" << std::endl;
        std::cerr << "Error: " << strerror(errno) << std::endl;
        if (shmctl(shmid, IPC_RMID, nullptr) == -1) {
            std::cerr << "Error removing shared memory segment" << std::endl;
            std::cerr << "Error: " << strerror(errno) << std::endl;
        }
        exit(1);
    }

     // Create a semaphore, only one process can access the shared memory (carsInStreet) at a time
    sem_t* semaphore;

    // Use sem_open cause it's shared between processes
    semaphore = sem_open("/my_semaphore", O_CREAT | O_EXCL, 0666, 1);
    if (semaphore == SEM_FAILED) {
        std::cerr << "Error creating semaphore" << std::endl;
        std::cerr << "Error: " << strerror(errno) << std::endl;
        if (sem_unlink("/my_semaphore") != 0) {
            std::cerr << "Error deleting semaphore: " << strerror(errno) << std::endl;
        }
        exit(1);
    }

    // Attach the shared memory segment is now an array of longs
    long* carsInStreetSharedMemory = static_cast<long*>(shmat(shmid, NULL, 0));
    if (carsInStreetSharedMemory == (void*) -1) {
        std::cerr << "Error attaching shared memory segment" << std::endl;
        std::cerr << "Error: " << strerror(errno) << std::endl;
        exit(1);
    }

    // Initialize the shared memory
    for (int i = 0; i < TOTAL_STREETS; i++) {
        carsInStreetSharedMemory[i] = 0;
    }

    // print the shared memory
    for (int i = 0; i < TOTAL_STREETS; i++) {
        std::cout << "Street " << i + 1 << ": " << carsInStreetSharedMemory[i] << std::endl;
    }

    // Fork three child processes create all 25 cars
    std::cout << "Creating " << TOTAL_CARS << " child processes" << std::endl;
    for(int i = 0; i < TOTAL_CARS; i++) {
        pid_t pid = fork();

        if (pid == -1) {
            std::cerr << "Error creating child process" << std::endl;
            std::cerr << "Error: " << strerror(errno) << std::endl;
            exit(1);
        } else if (pid == 0) { // Child process
            // Generate a random number between 1 and TOTAL_STREETS
            srand(time(NULL) + getpid());
            long randomStreet = rand() % TOTAL_STREETS + 1;
            long totalCarsInRoundabout = 0;
            Car car(i + 1, randomStreet * 10); // Valid streets are 10, 20, 30, 40, 50, 60, 70
            std::cout << "New car created with id: " << car.id << " and queue number: " << car.queueNumber << std::endl;
            sem_wait(semaphore);
            std::cout << "Car modifying shared memory" << std::endl;
            carsInStreetSharedMemory[randomStreet] += 1;
            for (int i = 0; i < TOTAL_STREETS; i++) {
                totalCarsInRoundabout += carsInStreetSharedMemory[i];
            }
            std::cout << "Total cars in roundabout: " << totalCarsInRoundabout << std::endl;
            sem_post(semaphore);
            

            if (totalCarsInRoundabout > MAX_CARS) {
                long streetWithMostCars = 0;
                long maxCars = 0;
                sem_wait(semaphore);
                for (int i = 0; i < TOTAL_STREETS; i++) {
                    if (carsInStreetSharedMemory[i] > maxCars) {
                        maxCars = carsInStreetSharedMemory[i];
                        streetWithMostCars = i;
                    }
                }
                sem_post(semaphore);
                car.allowPass(maxCars, streetWithMostCars, mailbox);
            }
            std::cout << "Total messages" << mailbox.getNumPendingMessages() << std::endl;
            
            if (i == TOTAL_CARS - 1) { // Last car
                // Call the method to allow all cars to pass
                std:: cout << "Last car" << std::endl;
                std::map<long,long> carsInStreetCopy;
                sem_wait(semaphore);
                for (int i = 0; i < TOTAL_STREETS; i++) {
                    carsInStreetCopy.insert(std::pair<long,long>(i, carsInStreetSharedMemory[i]));
                }
                sem_post(semaphore);
                car.allowAllCarsToPass(carsInStreetCopy, mailbox); 
            }

            car.carWaitingTurn(mailbox);
            // End of child process
        }
    }

    for (int i = 0; i < TOTAL_CARS; i++) {
        std::cout << "Waiting for child process " << i + 1 << std::endl;
        wait(NULL);
    }

    // Detach the shared memory segment
    if (shmdt(carsInStreetSharedMemory) == -1) {
        std::cerr << "Error detaching shared memory segment" << std::endl;
        std::cerr << "Error: " << strerror(errno) << std::endl;
        exit(1);
    }

    // Remove the shared memory segment
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        std::cerr << "Error removing shared memory segment" << std::endl;
        std::cerr << "Error: " << strerror(errno) << std::endl;
        exit(1);
    }

    sem_close(semaphore);
    sem_unlink("/my_semaphore");

    /* code */
    return 0;
}