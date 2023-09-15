#include "Car.hpp"
#include "Mailbox.hpp"
#include "Message.hpp"

#include <iostream>
#include <map>
#include <fcntl.h>           /* For O_* constants */
#include <semaphore.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <unistd.h>


int main(int argc, char const *argv[]) {
    const long TOTAL_STREETS = 7;
    const long TOTAL_CARS = 100;
    const long MAX_CARS = 25;

    Mailbox mailbox;
    // Create a semaphore, only one process can access the shared memory (carsInStreet) at a time
    sem_t* semaphore;

    // Use sem_open cause it's shared between processes
    semaphore = sem_open("/my_semaphore", O_CREAT, 0666, 1); // Initial value of 1
    if (semaphore == SEM_FAILED) {
        std::cerr << "Error creating semaphore" << std::endl;
        std::cerr << "Error: " << strerror(errno) << std::endl;
        exit(1);
    }


    // Share this memory between processes
    const key_t SHM_KEY = 1234;
    // Calculate the maximum size of carsInStreet
    const size_t SHM_SIZE = sizeof(std::unordered_map<long, long>) + 
                            (sizeof(long) * MAX_CARS) + 
                            (sizeof(long) * TOTAL_STREETS);

    // Define the shared memory segment here
    int shmid = shmget(SHM_KEY, SHM_SIZE, IPC_CREAT | 0666); 
    if (shmid == -1) {
        std::cerr << "Error creating shared memory segment" << std::endl;
        std::cerr << "Error: " << strerror(errno) << std::endl;
        exit(1);
    }
    

    std::unordered_map<long, long>* carsInStreetSharedMemory = static_cast<std::unordered_map<long, long>*> (shmat(shmid, nullptr, 0));
    if (carsInStreetSharedMemory == reinterpret_cast<void*>(-1)) {
        std::cerr << "Error attaching shared memory segment" << std::endl;
        std::cerr << "Error: " << strerror(errno) << std::endl;
        exit(1);
    }

    // Initialize the shared memory
    for (long i = 1; i <= TOTAL_STREETS; i++) {
        (*carsInStreetSharedMemory)[i * 10] = 0;
    }
    

    // Fork three child processes create all 25 cars

    for(int i = 0; i < TOTAL_CARS; i++) {
        pid_t pid = fork();

        if (pid == -1) {
            std::cerr << "Error creating child process" << std::endl;
            std::cerr << "Error: " << strerror(errno) << std::endl;
            exit(1);
        } else if (pid == 0) { // Child process
            long randomStreet = (rand() % TOTAL_STREETS) + 1;
            Car car(i + 1, randomStreet * 10); // Valid streets are 10, 20, 30, 40, 50, 60, 70

            // Call the semaphore
            sem_wait(semaphore);
            // Update the shared memory
            (*carsInStreetSharedMemory)[randomStreet * 10]++;
            std::cout << "Child Process " << i + 1 << ": Counter = " << (*carsInStreetSharedMemory)[randomStreet * 10] << std::endl;
            sem_post(semaphore);

            int totalQueueMessages = mailbox.getNumPendingMessages();
            if (totalQueueMessages > 0) {
                std::cout << "Total messages in queue: " << totalQueueMessages << std::endl;
            }

            if (totalQueueMessages > MAX_CARS) {
                sem_wait(semaphore);
                // find the street with the most cars
                long streetWithMostCars = 0;
                long maxCars = 0;
                for (auto& street : *carsInStreetSharedMemory) {
                    if (street.second > maxCars) {
                        maxCars = street.second;
                        streetWithMostCars = street.first;
                    }
                }
                car.allowPass(maxCars, streetWithMostCars, mailbox);
                std::cout << "Street with most cars: " << streetWithMostCars << std::endl;
                sem_post(semaphore);
            }
            
            if (i == TOTAL_CARS - 1) { // Last car
                // Call the method to allow all cars to pass
                sem_wait(semaphore);
                // make a copy of the shared memory and send the copy to the method
                std::unordered_map<long, long> carsInStreetCopy;
                for (auto& street : *carsInStreetSharedMemory) {
                    carsInStreetCopy[street.first] = street.second;
                }
                car.allowAllCarstoPass(carsInStreetCopy, mailbox);
            } else {
                car.carWaitingTurn(mailbox); // Here the process will wait until it is allowed to exit the roundabout
            }
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
