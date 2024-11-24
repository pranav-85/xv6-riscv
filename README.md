# XV6 IPC Mechanisms Documentation
## Message Queues and Semaphores Implementation

## 1. Message Queue Implementation

### Core Data Structures

```c
struct msg {
  char* content;
  int size;
  struct msg* next;
};

struct msgqueue {
  struct msg* head;
  struct msg* tail;
  int max_msgs;    // Maximum number of messages in queue
  int curr_msgs;   // Current number of messages
  int max_size;    // Maximum size of each message
  int refs;        // Number of processes referencing this queue
  struct spinlock lock;
};

#define MAX_QUEUES 16
#define MAX_MSG_SIZE 256
#define MAX_MSGS_PER_QUEUE 32

struct msgqueue msgqueues[MAX_QUEUES];
struct spinlock msgqueue_lock;
```

### Key System Calls

#### 1. msgget()
- **Purpose**: Creates or gets access to a message queue
- **Implementation**:
  ```c
  acquire(&msgqueue_lock);
  
  // Find free queue
  int i;
  for(i = 0; i < MAX_QUEUES; i++) {
    if(msgqueues[i].refs == 0) {
      msgqueues[i].refs = 1;
      release(&msgqueue_lock);
      return i;
    }
  }
  
  release(&msgqueue_lock);
  return -1; // No free queues
  ```

#### 2. msgsend()
- **Purpose**: Sends a message to a queue
- **Implementation**:
  ```c
  int qid;
  char* content;
  int size;
  
  argint(0, &qid);
  argaddr(1, (uint64*)&content);
  argint(2, &size);
    
  if(qid < 0 || qid >= MAX_QUEUES || size > MAX_MSG_SIZE)
    return -1;
    
  struct msgqueue* q = &msgqueues[qid];
  acquire(&q->lock);
  
  if(q->refs == 0 || q->curr_msgs >= q->max_msgs) {
    release(&q->lock);
    return -1;
  }
  
  // Use kalloc() for memory allocation
  char* buf = kalloc();  // Allocate a page for message content
  if(buf == 0) {
    release(&q->lock);
    return -1;
  }
  
  struct msg* m = (struct msg*)kalloc();  // Allocate msg structure
  if(m == 0) {
    kfree(buf);
    release(&q->lock);
    return -1;
  }
  
  if(copyin(myproc()->pagetable, buf, (uint64)content, size) < 0) {
    kfree(buf);
    kfree((char*)m);
    release(&q->lock);
    return -1;
  }
  
  m->content = buf;
  m->size = size;
  m->next = 0;
  
  if(q->tail) {
    q->tail->next = m;
    q->tail = m;
  } else {
    q->head = q->tail = m;
  }
  
  q->curr_msgs++;
  release(&q->lock);
  wakeup(q);  // Wake up any waiting receivers
  
  return 0;
  ```

#### 3. msgrcv()
- **Purpose**: Receives a message from a queue
- **Implementation**:
  ```c
  int qid;
  char* buf;
  int size;
  
  argint(0, &qid);
  argaddr(1, (uint64*)&buf);
  argint(2, &size);
    
  if(qid < 0 || qid >= MAX_QUEUES || size < 0)
    return -1;
    
  struct msgqueue* q = &msgqueues[qid];
  acquire(&q->lock);
  
  while(q->curr_msgs == 0 && q->refs > 0) {
    sleep(q, &q->lock);
  }
  
  if(q->refs == 0) {
    release(&q->lock);
    return -1;
  }
  
  struct msg* m = q->head;
  if(m == 0) {
    release(&q->lock);
    return -1;
  }
  
  int copy_size = m->size < size ? m->size : size;
  if(copyout(myproc()->pagetable, (uint64)buf, m->content, copy_size) < 0) {
    release(&q->lock);
    return -1;
  }
  
  q->head = m->next;
  if(q->head == 0)
    q->tail = 0;
  
  q->curr_msgs--;
  
  kfree(m->content);
  kfree((char*)m);
  
  release(&q->lock);
  return copy_size;
  ```

#### 4. msgclose()
- **Purpose**: Closes access to a message queue
- **Implementation**:
  ```c
  int qid;
  
  argint(0, &qid);
    
  if(qid < 0 || qid >= MAX_QUEUES)
    return -1;
    
  struct msgqueue* q = &msgqueues[qid];
  acquire(&q->lock);
  
  if(q->refs <= 0) {
    release(&q->lock);
    return -1;
  }
  
  q->refs--;
  
  if(q->refs == 0) {
    // Free all remaining messages
    struct msg* m = q->head;
    while(m) {
      struct msg* next = m->next;
      kfree(m->content);
      kfree((char*)m);
      m = next;
    }
    q->head = q->tail = 0;
    q->curr_msgs = 0;
  }
  
  release(&q->lock);
  return 0;
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
