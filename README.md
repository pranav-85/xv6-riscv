# XV6 IPC Mechanisms Documentation
## Message Queues and Semaphores Implementation

## 1. Message Queue Implementation

### Core Data Structures

```c
// Message Queue Structure
struct msg_queue {
  char* items[MAX_QUEUE_SIZE];  // Array to store messages
  int front;                    // Index for dequeuing
  int rear;                     // Index for enqueuing
  int size;                     // Current number of messages
  int ref_count;               // Number of processes using the queue
  struct spinlock lock;        // Lock for synchronization
};

// Global array of message queues
struct msg_queue msg_queues[MAX_QUEUES];
```

### Key System Calls

#### 1. msgget()
- **Purpose**: Creates or gets access to a message queue
- **Implementation**:
  ```c
  int
  msgget(void)
  {
    // Find first available queue slot
    for(int i = 0; i < MAX_QUEUES; i++) {
      if(msg_queues[i].ref_count == 0) {
        // Initialize queue
        msg_queues[i].front = 0;
        msg_queues[i].rear = 0;
        msg_queues[i].size = 0;
        msg_queues[i].ref_count = 1;
        initlock(&msg_queues[i].lock, "msgq");
        return i;  // Return queue ID
      }
    }
    return -1;    // No available queues
  }
  ```

#### 2. msgsend()
- **Purpose**: Sends a message to a queue
- **Implementation**:
  ```c
  int
  msgsend(int qid, char* msg, int size)
  {
    if(qid < 0 || qid >= MAX_QUEUES)
      return -1;
    
    struct msg_queue *q = &msg_queues[qid];
    acquire(&q->lock);
    
    // Check if queue is full
    if(q->size == MAX_QUEUE_SIZE) {
      release(&q->lock);
      return -1;
    }
    
    // Copy message to queue
    char* new_msg = kalloc();
    memmove(new_msg, msg, size);
    q->items[q->rear] = new_msg;
    q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
    q->size++;
    
    release(&q->lock);
    return 0;
  }
  ```

#### 3. msgrcv()
- **Purpose**: Receives a message from a queue
- **Implementation**:
  ```c
  int
  msgrcv(int qid, char* buf, int size)
  {
    if(qid < 0 || qid >= MAX_QUEUES)
      return -1;
    
    struct msg_queue *q = &msg_queues[qid];
    acquire(&q->lock);
    
    // Check if queue is empty
    if(q->size == 0) {
      release(&q->lock);
      return -1;
    }
    
    // Copy message from queue to user buffer
    char* msg = q->items[q->front];
    int msg_size = strlen(msg);
    if(msg_size > size)
      msg_size = size;
    memmove(buf, msg, msg_size);
    
    // Free queue slot
    kfree(msg);
    q->front = (q->front + 1) % MAX_QUEUE_SIZE;
    q->size--;
    
    release(&q->lock);
    return msg_size;
  }
  ```

#### 4. msgclose()
- **Purpose**: Closes access to a message queue
- **Implementation**:
  ```c
  int
  msgclose(int qid)
  {
    if(qid < 0 || qid >= MAX_QUEUES)
      return -1;
    
    struct msg_queue *q = &msg_queues[qid];
    acquire(&q->lock);
    
    // Free any remaining messages
    while(q->size > 0) {
      kfree(q->items[q->front]);
      q->front = (q->front + 1) % MAX_QUEUE_SIZE;
      q->size--;
    }
    
    q->ref_count--;
    release(&q->lock);
    return 0;
  }
  ```

## 2. Semaphore Implementation

### Core Data Structures

```c
// Semaphore Structure
struct semaphore {
  int value;                  // Semaphore counter
  char name[16];             // Semaphore name
  struct spinlock lock;      // Lock for synchronization
  struct proc *waiting[64];  // Array of waiting processes
  int wait_count;           // Number of waiting processes
  int ref_count;            // Number of processes using semaphore
};

// Global array of semaphores
struct semaphore semaphores[MAX_SEMS];
```

### Key System Calls

#### 1. sem_create()
- **Purpose**: Creates a new semaphore
- **Implementation**:
  ```c
  int
  sem_create(char *name, int value)
  {
    // Find available semaphore slot
    for(int i = 0; i < MAX_SEMS; i++) {
      if(semaphores[i].ref_count == 0) {
        // Initialize semaphore
        semaphores[i].value = value;
        strncpy(semaphores[i].name, name, 15);
        semaphores[i].name[15] = 0;
        semaphores[i].wait_count = 0;
        semaphores[i].ref_count = 1;
        initlock(&semaphores[i].lock, "sem");
        return i;  // Return semaphore ID
      }
    }
    return -1;    // No available semaphores
  }
  ```

#### 2. sem_wait()
- **Purpose**: Decrements semaphore value or blocks if zero
- **Implementation**:
  ```c
  int
  sem_wait(int sid)
  {
    if(sid < 0 || sid >= MAX_SEMS)
      return -1;
    
    struct semaphore *s = &semaphores[sid];
    acquire(&s->lock);
    
    while(s->value <= 0) {
      // Add process to waiting list
      s->waiting[s->wait_count++] = myproc();
      sleep(myproc(), &s->lock);
    }
    
    s->value--;
    release(&s->lock);
    return 0;
  }
  ```

#### 3. sem_signal()
- **Purpose**: Increments semaphore value and wakes waiting process
- **Implementation**:
  ```c
  int
  sem_signal(int sid)
  {
    if(sid < 0 || sid >= MAX_SEMS)
      return -1;
    
    struct semaphore *s = &semaphores[sid];
    acquire(&s->lock);
    
    s->value++;
    
    // Wake up one waiting process
    if(s->wait_count > 0) {
      wakeup(s->waiting[--s->wait_count]);
    }
    
    release(&s->lock);
    return 0;
  }
  ```

#### 4. sem_delete()
- **Purpose**: Deletes a semaphore
- **Implementation**:
  ```c
  int
  sem_delete(int sid)
  {
    if(sid < 0 || sid >= MAX_SEMS)
      return -1;
    
    struct semaphore *s = &semaphores[sid];
    acquire(&s->lock);
    
    // Wake up all waiting processes
    while(s->wait_count > 0) {
      wakeup(s->waiting[--s->wait_count]);
    }
    
    s->ref_count--;
    release(&s->lock);
    return 0;
  }
  ```

## 3. Key Implementation Notes

### Message Queue Features:
1. **Circular Buffer**: Implements a circular buffer to efficiently use memory
2. **Reference Counting**: Tracks number of processes using the queue
3. **Lock Protection**: Uses spinlocks to protect queue operations
4. **Dynamic Memory**: Allocates/frees memory for messages dynamically

### Semaphore Features:
1. **Named Semaphores**: Each semaphore can have a descriptive name
2. **Process Queue**: Maintains list of waiting processes
3. **FIFO Ordering**: Processes are woken in first-in-first-out order
4. **Reference Counting**: Tracks number of processes using the semaphore

### Synchronization Mechanisms:
1. **Spinlocks**: Used for short-term synchronization
2. **Sleep/Wakeup**: Used for longer-term blocking
3. **Atomic Operations**: Ensures thread-safety of operations

### Error Handling:
1. Range checking on queue and semaphore IDs
2. Memory allocation failure detection
3. Queue full/empty condition handling
4. Invalid operation detection

## 4. Usage Guidelines

### Message Queues:
1. Always check return values for errors
2. Close queues when done to prevent resource leaks
3. Keep messages smaller than MAX_MSG_SIZE
4. Handle queue full/empty conditions appropriately

### Semaphores:
1. Initialize with appropriate starting values
2. Always pair wait() with signal()
3. Clean up semaphores when done
4. Be careful of deadlock situations
