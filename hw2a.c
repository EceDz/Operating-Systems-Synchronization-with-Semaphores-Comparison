/******************************************************************************
Ece Düzgeç 20220702045
*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

// Variables for room capacity and pet counts
int MAXIMUM;                    
int total_dogs;                 
int total_cats;                  

// Semaphores for synchronization
sem_t mutex;                    // protects shared data
sem_t dogs_queue;               // blocks waiting dogs
sem_t cats_queue;               // blocks waiting cats

// Shared state variables
int dogs_in_room = 0;          
int cats_in_room = 0;           
int dogs_waiting = 0;          
int cats_waiting = 0;           
int dogs_served = 0;            // counter for dog batch quota
int cat_turn = 0;               // flag to give cats priority

#define CATS_AFTER_DOGS 5       // batch quota: after 5 dogs, cats get priority

typedef struct {
    int id;
    char type;  // 'D' for dogs or 'C' for cats
} 
pet_data;

struct timespec start_time, end_time;
void cat_wants_service(int cat_id);
void cat_leaves(int cat_id);
void dog_wants_service(int dog_id);
void dog_leaves(int dog_id);
void* pet_thread(void* arg);
void display_room();

// Display current room state
void display_room() {
    printf("\nService Room: Dogs: %d, Cats: %d",dogs_in_room, cats_in_room); 
    printf("\nWaiting: Dogs: %d, Cats: %d\n",dogs_waiting, cats_waiting);
}

// Cat tries to enter service room
void cat_wants_service(int cat_id) {
    sem_wait(&mutex);  // lock
    
    cats_waiting++;
    printf("\nCat %d arrives and wants service\n", cat_id);
    display_room();

    // Wait if the dogs are inside, room full or dogs waiting and not cat's turn
    while (dogs_in_room > 0 || 
           cats_in_room >= MAXIMUM ||
           (dogs_waiting > 0 && !cat_turn)) {
        sem_post(&mutex);  // unlock while waiting
        sem_wait(&cats_queue);  // sleep on queue
        sem_wait(&mutex);  // relock when woken up
    }
    
    // Enter room
    cats_waiting--;
    cats_in_room++;
    printf("\n+Cat %d enters service room\n", cat_id);
    display_room();
    
    sem_post(&mutex);  // unlock
}

// Cat finishes and leaves
void cat_leaves(int cat_id) {
    sem_wait(&mutex);  // lock
    
    cats_in_room--;
    printf("\n-Cat %d leaves service room\n", cat_id);
    display_room();
    
    // If last cat is leaving wake up the next pet
    if (cats_in_room == 0) {
        if (cats_waiting == 0) {
            cat_turn = 0;  // restore dog priority
        }
        
        // Priority to dogs if it is their turn then cats, then dogs again
        if (dogs_waiting > 0 && !cat_turn) {
            sem_post(&dogs_queue);
        } else if (cats_waiting > 0) {
            sem_post(&cats_queue);
        } else if (dogs_waiting > 0) {
            sem_post(&dogs_queue);
        }
    } 
    // Room is not empty but has space let another cat in
    else if (cats_in_room < MAXIMUM && cats_waiting > 0 && dogs_waiting == 0) {
        sem_post(&cats_queue);
    }
    
    sem_post(&mutex);  // unlock
}

// Dog tries to enter service room
void dog_wants_service(int dog_id) {
    sem_wait(&mutex);  // lock
    
    dogs_waiting++;
    printf("\nDog %d arrives and wants service\n", dog_id);
    display_room();
    
    // Wait if the cats are inside or room is full of dogs
    while (cats_in_room > 0 || dogs_in_room >= MAXIMUM) {
        sem_post(&mutex);  // unlock while waiting
        sem_wait(&dogs_queue);  // sleep on queue
        sem_wait(&mutex);  // relock when woken up
    }
    
    // Enter room
    dogs_waiting--;
    dogs_in_room++;
    dogs_served++;  // increment quota counter
    printf("\n+Dog %d enters service room\n", dog_id);
    display_room();
    
    sem_post(&mutex);  // unlock
}

// Dog finishes and leaves
void dog_leaves(int dog_id) {
    sem_wait(&mutex);  // lock
    
    dogs_in_room--;
    printf("\n-Dog %d leaves service room\n", dog_id);
    display_room();
    
    // If the last dog is leaving decide who goes next
    if (dogs_in_room == 0) {
        // Check if quota reached and cats are waiting
        if (dogs_served >= CATS_AFTER_DOGS && cats_waiting > 0) {
            dogs_served = 0;  // reset counter
            cat_turn = 1;     // give cats priority
            printf("Batch quota reached! Allowing cats to prevent starvation\n");
            sem_post(&cats_queue);
        } 
        // Otherwise priority to dogs then cats
        else if (dogs_waiting > 0) {
            sem_post(&dogs_queue);
        } else if (cats_waiting > 0) {
            sem_post(&cats_queue);
        }
    } 
    // Room is not empty but has space let another dog in
    else if (dogs_in_room < MAXIMUM && dogs_waiting > 0) {
        sem_post(&dogs_queue);
    }
    
    sem_post(&mutex);  // unlock
}

// Thread function for each pet
void* pet_thread(void* arg) {
    pet_data* data = (pet_data*)arg;

    sleep(rand() % 6);  // random arrival time
    
    if (data->type == 'D') {
        dog_wants_service(data->id);
        sleep(1);  // service time
        dog_leaves(data->id);
    } 
    else {
        cat_wants_service(data->id);
        sleep(1);  // service time
        cat_leaves(data->id);
    }
    
    free(data);
    return NULL;
}

int main(int argc, char* argv[]) {
    char continue_choice;
    srand(time(NULL));
    
    do {
        clock_gettime(CLOCK_MONOTONIC, &start_time); 
        
        // Get input from user
        printf("PET SHOP SERVICE hw2a.c");
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
        
        // Initialize semaphores
        sem_init(&mutex, 0, 1);      // unlocked
        sem_init(&dogs_queue, 0, 0); // locked
        sem_init(&cats_queue, 0, 0); // locked
        
        // Reset state
        dogs_in_room = 0;
        cats_in_room = 0;
        dogs_waiting = 0;
        cats_waiting = 0;
        dogs_served = 0;
        cat_turn = 0;
        
        // Create thread array
        pthread_t* threads = malloc((total_dogs + total_cats) * sizeof(pthread_t));
        int thread_idx = 0;
        
        // Create dog threads
        for (int i = 0; i < total_dogs; i++) {
            pet_data* data = malloc(sizeof(pet_data));
            data->id = i + 1;
            data->type = 'D';
            
            if (pthread_create(&threads[thread_idx++], NULL, pet_thread, data)) {
                printf("Error creating dog thread %d\n", i + 1);
                exit(1);
            }
        }
        
        // Create cat threads
        for (int i = 0; i < total_cats; i++) {
            pet_data* data = malloc(sizeof(pet_data));
            data->id = i + 1;
            data->type = 'C';
            
            if (pthread_create(&threads[thread_idx++], NULL, pet_thread, data)) {
                printf("Error creating cat thread %d\n", i + 1);
                exit(1);
            }
        }
        
        // Wait for all threads to finish
        for (int i = 0; i < total_dogs + total_cats; i++) {
            if (pthread_join(threads[i], NULL)) {
                printf("Error joining thread %d\n", i);
                exit(1);
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        
        // Calculate time
        double sim_seconds =(end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
            
        printf("\nAll pets have been served! Program complete:)");
        printf("\nProgram took: %.3f seconds\n", sim_seconds);
        
        // Cleanup
        sem_destroy(&mutex);
        sem_destroy(&dogs_queue);
        sem_destroy(&cats_queue);
        free(threads);
        
        printf("\nDo you want to continue? (y/n): ");
        scanf(" %c", &continue_choice);
        printf("\n");
        
    } while (continue_choice == 'y' || continue_choice == 'Y');
    
    printf("Exiting program... Goodbye :)\n");
    
    return 0;
}