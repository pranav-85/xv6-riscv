#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define BUFFER_SIZE 5
#define NUM_ITEMS 10

// Shared buffer
int buffer[BUFFER_SIZE];
int in = 0;   // index for inserting
int out = 0;  // index for removing

// Test function to demonstrate producer behavior
void
producer(int sem_empty, int sem_full, int sem_mutex)
{
  int item;
  
  for(item = 0; item < NUM_ITEMS; item++) {
    sem_wait(sem_empty);    // Wait for empty slot
    sem_wait(sem_mutex);    // Enter critical section
    
    // Produce item
    buffer[in] = item;
    printf("Producer: Inserted item %d at position %d\n", item, in);
    in = (in + 1) % BUFFER_SIZE;
    
    sem_signal(sem_mutex);  // Exit critical section
    sem_signal(sem_full);   // Signal that new item is in buffer
    
    sleep(10);  // Simulate some work
  }
  
  exit(0);
}

// Test function to demonstrate consumer behavior
void
consumer(int sem_empty, int sem_full, int sem_mutex)
{
  int item;
  int i;
  
  for(i = 0; i < NUM_ITEMS; i++) {
    sem_wait(sem_full);     // Wait for item to be available
    sem_wait(sem_mutex);    // Enter critical section
    
    // Consume item
    item = buffer[out];
    printf("Consumer: Removed item %d from position %d\n", item, out);
    out = (out + 1) % BUFFER_SIZE;
    
    sem_signal(sem_mutex);  // Exit critical section
    sem_signal(sem_empty);  // Signal that a slot is free
    
    sleep(20);  // Simulate some work
  }
  
  exit(0);
}

int
main(void)
{
  int pid;
  int sem_mutex, sem_empty, sem_full;
  
  // Create semaphores
  sem_mutex = sem_create("mutex", 1);      // Binary semaphore for mutual exclusion
  sem_empty = sem_create("empty", BUFFER_SIZE); // Counting semaphore for empty slots
  sem_full = sem_create("full", 0);        // Counting semaphore for full slots
  
  if(sem_mutex < 0 || sem_empty < 0 || sem_full < 0) {
    printf("Failed to create semaphores\n");
    exit(0);
  }
  
  printf("Starting producer-consumer test...\n");
  
  // Create producer process
  pid = fork();
  if(pid < 0) {
    printf("Fork failed\n");
    exit(0);
  }
  
  if(pid == 0) {  // Child process (Producer)
    producer(sem_empty, sem_full, sem_mutex);
  }
  
  // Create consumer process
  pid = fork();
  if(pid < 0) {
    printf("Fork failed\n");
    exit(0);
  }
  
  if(pid == 0) {  // Child process (Consumer)
    consumer(sem_empty, sem_full, sem_mutex);
  }
  
  // Parent process waits for both children
  wait(0);
  wait(0);
  
  // Clean up semaphores
  sem_delete(sem_mutex);
  sem_delete(sem_empty);
  sem_delete(sem_full);
  
  printf("Producer-consumer test completed\n");
  exit(0);
}