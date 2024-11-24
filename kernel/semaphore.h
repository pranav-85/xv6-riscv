#include "spinlock.h"
#include "param.h"
#define MAX_SEMAPHORES 32
#define SEM_NAME_LEN   16

struct semaphore {
  int value;                    // Current semaphore value
  struct spinlock lock;         // Lock for atomic operations
  struct proc *waiting[NPROC];  // Array of waiting processes
  int nwaiting;                // Number of waiting processes
  int inuse;                   // Whether this semaphore slot is in use
  char name[SEM_NAME_LEN];     // Name of the semaphore
};