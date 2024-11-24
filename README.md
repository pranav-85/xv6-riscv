# XV6 System Calls Documentation

---

## **Steps to Add a System Call in xv6**

---

#### **1. Define the System Call Function in the Kernel**

Write the function that implements your system call in one of the kernel files (e.g., `sysproc.c` or a relevant file based on the purpose of the system call).  
- **Example:** Add the `sys_rename` function in `sysfile.c`.

```c
// In sysfile.c or appropriate kernel file
int sys_rename(void) {
    // Your implementation here
    return 0; // Example: Replace with actual functionality
}
```

---

#### **2. Add an Entry in the System Call Table**

Update the `syscall.c` file to link the system call number to your function.  
- **File:** `syscall.c`  
- **Action:** Add your function to the `syscalls` array.

```c
extern int sys_rename(void); // Declare the function

static int (*syscalls[])(void) = {
    // Other system calls
    [SYS_rename] sys_rename,
};
```

---

#### **3. Assign a Unique System Call Number**

Define a unique identifier for the system call.  
- **File:** `kernel/syscall.h`  
- **Action:** Add a constant for the system call number.

```c
#define SYS_rename 22 // Use the next available number
```

---

#### **4. Update `syscall.c` to Parse Arguments**

If your system call takes arguments, use `argstr`, `argint`, or similar functions to fetch them.  
- **File:** `syscall.c`  
- **Action:** Map system call arguments to variables.

```c
if (argstr(0, oldpath, MAXPATH) < 0 || argstr(1, newpath, MAXPATH) < 0) {
    return -1; // Example: Fetch strings passed from user space
}
```

---

#### **5. Add a Prototype in `user.h`**

Declare the user-space interface for the system call.  
- **File:** `user/user.h`  
- **Action:** Add a prototype for the system call.

```c
int rename(const char *oldpath, const char *newpath);
```

---

#### **6. Update the `usys.pl` File**

Add your system call to `usys.pl` to generate the assembly stubs required for user programs.  
- **File:** `user/usys.pl`  
- **Action:** Add the name of the system call.

```perl
entry("rename");
```

Run `make` to regenerate `usys.S`.

---

#### **7. Implement a User Program**

Write a user-space program to test your system call.  
- **Example:**

```c
#include "kernel/types.h"
#include "user.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: rename oldpath newpath\n");
        exit(1);
    }

    if (rename(argv[1], argv[2]) < 0) {
        printf("Rename failed\n");
        exit(1);
    }

    printf("Rename succeeded\n");
    exit(0);
}
```

---

#### **8. Rebuild the xv6 Kernel**

Run the following commands to compile the kernel with the new system call:
```bash
make clean
make qemu
```

---

#### **9. Test the System Call**

Use the test program you wrote or call the system call directly in another user program.

---

### **Summary of Files to Modify**
1. **Kernel Implementation:**
   - `sysfile.c` (or relevant file): Write the system call implementation.
   - `syscall.c`: Add the function to the system call table.
   - `syscall.h`: Define a unique system call number.

2. **User Space:**
   - `user/user.h`: Declare the system call prototype.
   - `user/usys.pl`: Add the system call to the list.

3. **Testing:**
   - Write a user program for testing the system call.
     

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
```

### Key System Calls

#### 1. sem_create(char *name, int value)
- **Purpose**: Creates a new semaphore
- **Implementation**:
  ```c
  struct semaphore *s;
  int i;

  if(name == 0 || strlen(name) >= SEM_NAME_LEN)
    return -1;

  acquire(&stable.lock);

  // Check if semaphore already exists
  for(i = 0; i < MAX_SEMAPHORES; i++) {
    s = &stable.sems[i];
    if(s->inuse && strncmp(s->name, name, SEM_NAME_LEN) == 0) {
      release(&stable.lock);
      return i;
    }
  }

  // Find free slot
  for(i = 0; i < MAX_SEMAPHORES; i++) {
    s = &stable.sems[i];
    if(!s->inuse) {
      s->inuse = 1;
      s->value = value;
      s->nwaiting = 0;
      safestrcpy(s->name, name, SEM_NAME_LEN);
      release(&stable.lock);
      return i;
    }
  }

  release(&stable.lock);
  return -1;
  ```

#### 2. sem_wait(sem_id)
- **Purpose**: Decrements semaphore value or blocks if zero
- **Implementation**:
  ```c
  struct semaphore *s;
  struct proc *p = myproc();

  if(sem_id < 0 || sem_id >= MAX_SEMAPHORES)
    return;

  s = &stable.sems[sem_id];
  acquire(&s->lock);

  while(s->value <= 0) {
    if(s->nwaiting < NPROC) {
      s->waiting[s->nwaiting++] = p;
      sleep(p, &s->lock);  // Releases lock while sleeping
    }
    // Re-acquire lock after wakeup
    if(!s->inuse) {
      release(&s->lock);
      return;
    }
  }

  s->value--;
  release(&s->lock);
  ```

#### 3. sem_signal(int sem_id)
- **Purpose**: Increments semaphore value and wakes waiting process
- **Implementation**:
  ```c
  struct semaphore *s;
  struct proc *p;

  if(sem_id < 0 || sem_id >= MAX_SEMAPHORES)
    return;

  s = &stable.sems[sem_id];
  acquire(&s->lock);

  s->value++;

  if(s->nwaiting > 0) {
    p = s->waiting[--s->nwaiting];
    wakeup(p);
  }

  release(&s->lock);
  ```

#### 4. sem_delete(int sem_id)
- **Purpose**: Deletes a semaphore
- **Implementation**:
  ```c
  struct semaphore *s;

  if(sem_id < 0 || sem_id >= MAX_SEMAPHORES)
    return -1;

  s = &stable.sems[sem_id];
  acquire(&s->lock);

  if(!s->inuse) {
    release(&s->lock);
    return -1;
  }

  while(s->nwaiting > 0) {
    wakeup(s->waiting[--s->nwaiting]);
  }

  s->inuse = 0;
  release(&s->lock);
  return 0;
  ```


## **System Call: `rename`**

---

#### **Purpose**  
Rename a file or directory from an old path to a new path.

---

#### **Usage**  
In user programs, the `rename` system call is used as:  
```c
int rename(const char *oldpath, const char *newpath);
```

**Parameters:**  
- `oldpath`: The current path of the file or directory to rename.  
- `newpath`: The desired new path for the file or directory.  

**Returns:**  
- `0` on success.  
- `-1` on failure.  

**Example:**  
```c
if (rename("oldfile.txt", "newfile.txt") < 0) {
    printf("Rename failed\n");
}
```

---

#### **Code**
```c
int sys_rename(void) {
  char oldpath[MAXPATH], newpath[MAXPATH];
  struct inode *old_ip = 0, *new_ip = 0;
  struct inode *dp = 0, *old_dp = 0;
  char name[DIRSIZ], oldname[DIRSIZ];
  struct dirent de;
  int off;

  if (argstr(0, oldpath, MAXPATH) < 0 || argstr(1, newpath, MAXPATH) < 0) {
    return -1;
  }

  begin_op();

  if ((old_ip = namei(oldpath)) == 0) {
    end_op();
    return -1;
  }
  ilock(old_ip);

  if ((new_ip = namei(newpath)) != 0) {
    iunlockput(old_ip);
    iput(new_ip);
    end_op();
    return -1;
  }

  if ((old_dp = nameiparent(oldpath, oldname)) == 0 || 
      (dp = nameiparent(newpath, name)) == 0) {
    iput(old_dp);
    iunlockput(old_ip);
    end_op();
    return -1;
  }

  if (old_ip->type == T_DIR && 
      (namecmp(oldname, ".") == 0 || namecmp(oldname, "..") == 0)) {
    iput(old_dp);
    iput(dp);
    iunlockput(old_ip);
    end_op();
    return -1;
  }

  if (old_dp < dp) {
    ilock(old_dp);
    ilock(dp);
  } else {
    ilock(dp);
    if (old_dp != dp)
      ilock(old_dp);
  }

  if (old_dp->dev != dp->dev || old_dp->dev != old_ip->dev) {
    iunlockput(old_dp);
    if (old_dp != dp) {
      iunlockput(dp);
    }
    iunlockput(old_ip);
    end_op();
    return -1;
  }

  if (dirlink(dp, name, old_ip->inum) < 0) {
    iunlockput(old_dp);
    if (old_dp != dp) {
      iunlockput(dp);
    }
    iunlockput(old_ip);
    end_op();
    return -1;
  }

  for (off = 0; off < old_dp->size; off += sizeof(de)) {
    if (readi(old_dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("rename: readi");
    if (de.inum != 0 && namecmp(de.name, oldname) == 0) {
      de.inum = 0;
      if (writei(old_dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
        panic("rename: writei");
      break;
    }
  }

  if (old_dp != dp) {
    iunlockput(old_dp);
  }
  iunlockput(dp);
  iunlockput(old_ip);

  end_op();
  return 0;
}
```

---

### 4. **System Call: `createfile`**

---

#### **Purpose**  
Creates a new file. If a file with the same name already exists, automatically generates a numbered version.

---

#### **Usage**  
In user programs, the `createfile` system call is used as:  
```c
int createfile(const char *filename);
```

**Parameters:**  
- `filename`: The desired name of the file to create.  

**Returns:**  
- File descriptor (`int`) on success.  
- `-1` on failure.  

**Example:**  
```c
int fd = createfile("newfile.txt");
if (fd < 0) {
    printf("File creation failed\n");
} else {
    printf("File created successfully\n");
    close(fd);
}
```

---

#### **Code**
```c
int sys_createfile(void) {
  char path[MAXPATH];
  struct file *f;
  int fd;

  if (argstr(0, path, MAXPATH) < 0)
    return -1;

  fd = -1;
  f = filealloc();
  if (f == 0)
    return -1;

  f->ip = create(path, T_FILE, 0, 0);
  if (f->ip == 0) {
    fileclose(f);
    return -1;
  }
  f->type = FD_INODE;
  f->off = 0;
  f->readable = 1;
  f->writable = 1;

  if ((fd = fdalloc(f)) < 0) {
    fileclose(f);
    return -1;
  }

  return fd;
}
```

Here's the **full markdown document** with detailed, well-structured steps for each stage:
```
`# Adding `sigint` System Call in xv6
```
This document explains the steps to add a new system call, `sigint`, to xv6. The `sigint` system call terminates the current process by marking it as killed.


## Overview

The `sigint` system call is designed to terminate the calling process by marking it as killed. This document outlines the steps required to implement, integrate, and test this system call in the xv6 operating system.

---

## Steps to Implement

### 1. Modify the Kernel

The implementation of the `sigint` system call resides in `sysproc.c`. This function retrieves the current process, logs its termination, and marks it as killed.

#### Code in `sysproc.c`:
```c
uint64 sys_sigint(void) {
    struct proc *p = myproc();  // Get the current process

    if (p) {
        printf("SIGINT: Terminating process %d\n", p->pid);
        p->killed = 1;  // Mark the process as killed
    }
    return 0;  // Return 0 on success
} 
```

* * * * *

### 2\. Update the System Call Table

To link the system call number to its kernel implementation, update the `syscall.c` file:

1.  **Declare the New System Call Function**\
    Add the following declaration at the top of `syscall.c`:

```
    `extern uint64 sys_sigint(void);`
```
2.  **Add the `sys_sigint` Function to the System Call Array**\
    Map the system call number (`SYS_sigint`) to the `sys_sigint` function:

``` c
    `[SYS_sigint] sys_sigint,`
```
This links the system call number to its implementation in the kernel.
### 3\. Assign a Unique System Call Number

In `syscall.h`, define a unique system call number for `sigint`. Ensure this number does not conflict with existing system calls.

#### Code in `syscall.h`:
```c
`#define SYS_sigint 23`
```
### 4\. Add a User-Space Interface

To enable user programs to call the `sigint` system call, add its prototype and generate the required assembly stubs:

1.  **Add the Prototype in `user/user.h`**\
    Declare the `sigint` function as follows:
``` c
    `int sigint(void);`
```
2.  **Update `usys.pl`**\
    Add an entry for `sigint` to the list of system calls in `usys.pl`:

    perl
``` 
    `entry("sigint");`
``` 
    Run `make` to regenerate the `usys.S` file.
    
* * * * *

### 5\. Create a Test Program

Create a test program to verify the functionality of the `sigint` system call. This program attempts to call `sigint` and terminate itself.

#### Code in `sigint_test.c` (in the `user` directory):
``` c
`#include "user.h"

int main() {
    printf("Testing SIGINT system call\n");
    sigint();  // Call the SIGINT system call to terminate
    printf("Returned from SIGINT system call\n");  // This will not execute if sigint terminates
    exit(0);
}
```

* * * * *

### 6\. Update the Makefile

Add the test program to the list of user binaries in the `Makefile`:

#### Code in `Makefile`:

```
`$U/_sigint_test\`
```
* * * * *

### 7\. Build and Test

1.  **Rebuild the xv6 Kernel**:\
    Run the following commands to clean, build, and start xv6:

    bash
```
    make clean
    make qemu
```
2.  **Run the Test Program**:\
    Inside the xv6 shell, execute the test program:

    bash

   ```
    ./sigint_test
```
    **Expected Output**:

    `Testing SIGINT system call
    SIGINT: Terminating process <pid>`

    The message `Returned from SIGINT system call` will not appear since the process is terminated by the `sigint` system call.

* * * * *

Summary of Changes
------------------

### Kernel Changes

-   **`sysproc.c`**: Implemented the `sys_sigint` function.
-   **`syscall.c`**: Declared `sys_sigint` and added it to the `syscalls` array.
-   **`syscall.h`**: Defined the `SYS_sigint` constant.

### User-Space Changes

-   **`user/user.h`**: Added the prototype for `sigint`.
-   **`user/usys.pl`**: Added an entry for `sigint` to generate assembly stubs.
-   **`sigint_test.c`**: Created a user-space program to test the system call.

### Build System

-   **`Makefile`**: Added `sigint_test` to the user binaries.

* * * * *

Conclusion
----------

The `sigint` system call was successfully added to xv6. This process demonstrates how to implement a new system call, integrate it into the kernel, and test it with a user program. The steps outlined here can serve as a reference for adding similar functionality in xv6.

```
 `This markdown document includes every step in detail, with clear explanations and code snippe```
