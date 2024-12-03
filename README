# XV6 System Calls  Documentation

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
