/******************************************************************************
Ece Düzgeç 20220702045
*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <time.h>
#include <string.h>

// Shared state structure for shared memory
typedef struct {
    int dogs_in_room;
    int cats_in_room;
    int dogs_waiting;
    int cats_waiting;
    int dogs_served;    // counter for dog batch quota
    int MAXIMUM;
    int cat_turn;       // flag to give cats priority
} 
shared_state;

struct timespec start_time, end_time;

// Shared memory ids
int shm_state_id
int shm_mutex_id
int shm_dogs_queue_id
int shm_cats_queue_id;

// Pointers to shared memory segments
shared_state* state;
sem_t* mutex;
sem_t* dogs_queue;
sem_t* cats_queue;

#define CATS_AFTER_DOGS 5  // batch quota: after 5 dogs, cats get priority

void cat_wants_service(int cat_id);
void cat_leaves(int cat_id);
void dog_wants_service(int dog_id);
void dog_leaves(int dog_id);
void display_room();
void cleanup_resources();

// Display current room state
void display_room() {
    printf("\nService Room: Dogs: %d, Cats: %d",state->dogs_in_room, state->cats_in_room); 
    printf("\nWaiting: Dogs: %d, Cats: %d\n",state->dogs_waiting, state->cats_waiting);
    fflush(stdout);
}

// Cat tries to enter service room
void cat_wants_service(int cat_id) {
    sem_wait(mutex);  // lock
    
    state->cats_waiting++;
    printf("\nCat %d arrives and wants service\n", cat_id);
    fflush(stdout);
    display_room();
    
    // Wait if the dogs are inside, room full or dogs waiting and not cat's turn
    while (state->dogs_in_room > 0 || 
           state->cats_in_room >= state->MAXIMUM ||
           (state->dogs_waiting > 0 && !state->cat_turn)) {
        sem_post(mutex);  // unlock while waiting
        sem_wait(cats_queue);  // sleep on queue
        sem_wait(mutex);  // relock when woken up
    }
    
    // Enter room
    state->cats_waiting--;
    state->cats_in_room++;
    printf("\n+Cat %d enters service room\n", cat_id);
    fflush(stdout);
    display_room();
    
    sem_post(mutex);  // unlock
}

// Cat finishes and leaves
void cat_leaves(int cat_id) {
    sem_wait(mutex);  // lock
    
    state->cats_in_room--;
    printf("\n-Cat %d leaves service room\n", cat_id);
    fflush(stdout);
    display_room();
    
    // If last cat leaving, wake up next pet
    if (state->cats_in_room == 0) {
        if (state->cats_waiting == 0) {
            state->cat_turn = 0;  // restore dog priority
        }
        
        // Priority: dogs (if their turn), then cats, then dogs again
        if (state->dogs_waiting > 0 && !state->cat_turn) {
            sem_post(dogs_queue);
        } else if (state->cats_waiting > 0) {
            sem_post(cats_queue);
        } else if (state->dogs_waiting > 0) {
            sem_post(dogs_queue);
        }
    } 
    // Room not empty but has space let another cat in
    else if (state->cats_in_room < state->MAXIMUM && 
               state->cats_waiting > 0 && state->dogs_waiting == 0) {
        sem_post(cats_queue);
    }
    
    sem_post(mutex);  // unlock
}

// Dog tries to enter service room
void dog_wants_service(int dog_id) {
    sem_wait(mutex);  // lock
    
    state->dogs_waiting++;
    printf("\nDog %d arrives and wants service\n", dog_id);
    fflush(stdout);
    display_room();
    
    // Wait if the are cats inside or room full of dogs
    while (state->cats_in_room > 0 || state->dogs_in_room >= state->MAXIMUM) {
        sem_post(mutex);  // unlock while waiting
        sem_wait(dogs_queue);  // sleep on queue
        sem_wait(mutex);  // relock when woken up
    }
    
    // Enter room
    state->dogs_waiting--;
    state->dogs_in_room++;
    state->dogs_served++;  // increment quota counter
    printf("\n+Dog %d enters service room\n", dog_id);
    fflush(stdout);
    display_room();
    
    sem_post(mutex);  // unlock
}

// Dog finishes and leaves
void dog_leaves(int dog_id) {
    sem_wait(mutex);  // lock
    
    state->dogs_in_room--;
    printf("\n-Dog %d leaves service room\n", dog_id);
    fflush(stdout);
    display_room();
    
    // If last dog leaving decide who goes next
    if (state->dogs_in_room == 0) {
        // Check if quota reached and cats are waiting
        if (state->dogs_served >= CATS_AFTER_DOGS && state->cats_waiting > 0) {
            state->dogs_served = 0;  // reset counter
            state->cat_turn = 1;     // give cats priority
            printf("Batch quota reached! Allowing cats to prevent starvation\n");
            fflush(stdout);
            sem_post(cats_queue);
        } 
        // Otherwise priority to dogs then cats
        else if (state->dogs_waiting > 0) {
            sem_post(dogs_queue);
        } else if (state->cats_waiting > 0) {
            sem_post(cats_queue);
        }
    } 
    // Room not empty but has space let another dog in
    else if (state->dogs_in_room < state->MAXIMUM && state->dogs_waiting > 0) {
        sem_post(dogs_queue);
    }
    
    sem_post(mutex);  // unlock
}

// Clean up all shared memory and semaphores
void cleanup_resources() {
    shmdt((void*)state);
    shmctl(shm_state_id, IPC_RMID, NULL);
    
    shmdt((void*)mutex);
    shmctl(shm_mutex_id, IPC_RMID, NULL);
    
    shmdt((void*)dogs_queue);
    shmctl(shm_dogs_queue_id, IPC_RMID, NULL);
    
    shmdt((void*)cats_queue);
    shmctl(shm_cats_queue_id, IPC_RMID, NULL);
}

// Process function for each pet
void pet_process(char type, int id) {
    sleep(rand() % 6);  // random arrival time
    
    if (type == 'D') {
        dog_wants_service(id);
        sleep(1);  // service time
        dog_leaves(id);
    } else {
        cat_wants_service(id);
        sleep(1);  // service time
        cat_leaves(id);
    }
    
    exit(0);  // child process exits
}

int main(int argc, char* argv[]) {
    int MAXIMUM, total_dogs, total_cats;
    char continue_choice;
    srand(time(NULL));
    
     do {
        clock_gettime(CLOCK_MONOTONIC, &start_time);  
        
        // Get input from user
        printf("PET SHOP SERVICE hw2b.c");
        printf("\nEnter service room capacity (MAXIMUM > 0):");
        fflush(stdout);
        while (scanf("%d", &MAXIMUM) != 1 || MAXIMUM <= 0) {
            while (getchar() != '\n'); 
            printf("\nError: Invalid input. Please enter a number > 0.");
            printf("\nEnter service room capacity (MAXIMUM > 0): ");
            fflush(stdout);
        }
        printf("\nEnter number of dogs (>= 0): ");
        fflush(stdout);
        while (scanf("%d", &total_dogs) != 1 || total_dogs < 0) {
            while (getchar() != '\n'); 
            printf("\nError: Invalid input. Please enter a number >= 0.");
            printf("\nEnter number of dogs (>= 0): ");
            fflush(stdout);
        }
        
        printf("\nEnter number of cats (>= 0): ");
        fflush(stdout);
        while (scanf("%d", &total_cats) != 1 || total_cats < 0) {
            while (getchar() != '\n'); 
            printf("\nError: Invalid input. Please enter a number >= 0.");
            printf("\nEnter number of cats (>= 0): ");
            fflush(stdout);
        }
        
        printf("\nStarting simulation with:\n");
        printf("Service room capacity: %d\n", MAXIMUM);
        printf("Number of dogs: %d\n", total_dogs);
        printf("Number of cats: %d\n", total_cats);
        printf("Dog batch quota: %d\n", CATS_AFTER_DOGS);
        
        // Create shared memory for state
        shm_state_id = shmget((key_t)5001, sizeof(shared_state), IPC_CREAT | 0666);
        if (shm_state_id == -1) {
            perror("shmget failed for state");
            exit(1);
        }
        state = (shared_state*)shmat(shm_state_id, NULL, 0);
        if (state == (shared_state*)-1) {
            perror("shmat failed for state");
            exit(1);
        }

        // Initialize state
        state->dogs_in_room = 0;
        state->cats_in_room = 0;
        state->dogs_waiting = 0;
        state->cats_waiting = 0;
        state->dogs_served = 0;
        state->MAXIMUM = MAXIMUM;
        state->cat_turn = 0;
        
        // Create shared memory for mutex semaphore
        shm_mutex_id = shmget((key_t)5002, sizeof(sem_t), IPC_CREAT | 0666);
        if (shm_mutex_id == -1) {
            perror("shmget failed for mutex");
            exit(1);
        }
        mutex = (sem_t*)shmat(shm_mutex_id, NULL, 0);
        if (mutex == (sem_t*)-1) {
            perror("shmat failed for mutex");
            exit(1);
        }
        sem_init(mutex, 1, 1);  // pshared=1 for process sharing, initial=1 (unlocked)
        
        // Create shared memory for dogs queue semaphore
        shm_dogs_queue_id = shmget((key_t)5003, sizeof(sem_t), IPC_CREAT | 0666);
        if (shm_dogs_queue_id == -1) {
            perror("shmget failed for dogs_queue");
            exit(1);
        }
        dogs_queue = (sem_t*)shmat(shm_dogs_queue_id, NULL, 0);
        if (dogs_queue == (sem_t*)-1) {
            perror("shmat failed for dogs_queue");
            exit(1);
        }
        sem_init(dogs_queue, 1, 0);  // initial=0 (locked)
        
        // Create shared memory for cats queue semaphore
        shm_cats_queue_id = shmget((key_t)5004, sizeof(sem_t), IPC_CREAT | 0666);
        if (shm_cats_queue_id == -1) {
            perror("shmget failed for cats_queue");
            exit(1);
        }
        cats_queue = (sem_t*)shmat(shm_cats_queue_id, NULL, 0);
        if (cats_queue == (sem_t*)-1) {
            perror("shmat failed for cats_queue");
            exit(1);
        }
        sem_init(cats_queue, 1, 0);  // initial=0 (locked)
        
        // Fork dog processes
        for (int i = 0; i < total_dogs; i++) {
            pid_t pid = fork();
            
            if (pid == -1) {
                perror("fork failed for dog");
                exit(1);
            } else if (pid == 0) {
                // Child process
                pet_process('D', i + 1);
            }
            // Parent continues the loop
        }
        
        // Fork cat processes
        for (int i = 0; i < total_cats; i++) {
            pid_t pid = fork();
            
            if (pid == -1) {
                perror("fork failed for cat");
                exit(1);
            } else if (pid == 0) {
                // Child process
                pet_process('C', i + 1);
            }
            // Parent continues loop
        }
        
        // Wait for all child processes to finish
        for (int i = 0; i < total_dogs + total_cats; i++) {
            wait(NULL);
        }
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        
        // Calculate time
        double sim_seconds =(end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
            
        printf("\nAll pets have been served! Program complete:)");
        printf("\nProgram took: %.3f seconds\n", sim_seconds);
        
        // Cleanup shared memory
        cleanup_resources();
        
        printf("\nDo you want to continue? (y/n): ");
        scanf(" %c", &continue_choice);
        printf("\n");
        
    } while (continue_choice == 'y' || continue_choice == 'Y');
    
    printf("Exiting program... Goodbye :)\n");
    
    return 0;
}