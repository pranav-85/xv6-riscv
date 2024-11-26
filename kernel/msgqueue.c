#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"    
#include "proc.h"
#include "defs.h"

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

void
msgqueue_init(void)
{
  initlock(&msgqueue_lock, "msgqueues");
  for(int i = 0; i < MAX_QUEUES; i++) {
    initlock(&msgqueues[i].lock, "msgqueue");
    msgqueues[i].head = 0;
    msgqueues[i].tail = 0;
    msgqueues[i].max_msgs = MAX_MSGS_PER_QUEUE;
    msgqueues[i].curr_msgs = 0;
    msgqueues[i].max_size = MAX_MSG_SIZE;
    msgqueues[i].refs = 0;
  }
}

int
sys_msgget(void)
{
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
}

int
sys_msgsend(void)
{
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
}

int
sys_msgrcv(void)
{
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
}

int
sys_msgclose(void)
{
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
}