#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "proc.h"
#include "semaphore.h"

struct {
  struct spinlock lock;
  struct semaphore sems[MAX_SEMAPHORES];
} stable;

void
seminit(void)
{
  initlock(&stable.lock, "semaphore");
  for(int i = 0; i < MAX_SEMAPHORES; i++) {
    stable.sems[i].inuse = 0;
    initlock(&stable.sems[i].lock, "sem");
  }
}

uint64
sem_create(char *name, int value)
{
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
}

void
sem_wait(int sem_id)
{
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
}

void
sem_signal(int sem_id)
{
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
}

uint64
sem_delete(int sem_id)
{
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
}

// System call implementations
uint64
sys_sem_create(void)
{
  char name[64];
  int value;

  if(argstr(0, name, sizeof(name)) < 0)
    return -1;
  argint(1, &value);

  return sem_create(name, value);
}

uint64
sys_sem_wait(void)
{
  int sem_id;

  argint(0, &sem_id);

  sem_wait(sem_id);
  return 0;
}

uint64
sys_sem_signal(void)
{
  int sem_id;

  argint(0, &sem_id);

  sem_signal(sem_id);
  return 0;
}

uint64
sys_sem_delete(void)
{
  int sem_id;

  argint(0, &sem_id);

  return sem_delete(sem_id);
}