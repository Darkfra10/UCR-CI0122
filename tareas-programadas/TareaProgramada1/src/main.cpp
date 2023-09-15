#include <iostream>
#include <unistd.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/wait.h>
#include <map>


// Define the shared memory key and size
const key_t SHM_KEY = 1234;
const size_t SHM_SIZE = sizeof(int);

// Define the semaphore
sem_t* semaphore;

int main() {
    // Create a semaphore
    semaphore = sem_open("/my_semaphore", O_CREAT, 0666, 1); // Initial value of 1

    if (semaphore == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    int shmid;
    int* shared_counter;

    // Create or access the shared memory segment
    shmid = shmget(SHM_KEY, SHM_SIZE, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    // Attach the shared memory segment to the process's address space
    shared_counter = (int*)shmat(shmid, NULL, 0);
    if (shared_counter == (int*)-1) {
        perror("shmat");
        std::cerr << "Error: " << errno << std::endl;
        exit(1);
    }

    // Fork three child processes
    for (int i = 0; i < 3; i++) {
        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");
            exit(1);
        } else if (pid == 0) {
            // Child process
            sem_wait(semaphore); // Wait for semaphore
            (*shared_counter)++; // Update the shared counter
            std::cout << "Child Process " << i + 1 << ": Counter = " << (*shared_counter) << std::endl;
            sem_post(semaphore); // Release semaphore
            exit(0);
        }
    }

    // Wait for all child processes to finish
    for (int i = 0; i < 3; i++) {
        wait(NULL);
    }

    // Detach the shared memory segment
    if (shmdt(shared_counter) == -1) {
        perror("shmdt");
        exit(1);
    }

    // Remove the shared memory segment
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }

    // Close and unlink the semaphore
    sem_close(semaphore);
    sem_unlink("/my_semaphore");

    return 0;
}
