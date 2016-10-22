#include "thread.h"
#include "interrupt.h"
#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>

/* This is the thread control block */
struct thread {
  /* ... Fill this in ... */
  int free_state;
  ucontext_t context;
};

struct ready_queue {
  Tid thread_array_id;
  struct ready_queue *next_queue;
};
struct thread *thread_array[THREAD_MAX_THREADS];
struct ready_queue new_queue;
Tid current;
struct thread core_thread;

void thread_init(void) {
  int enabled = interrupts_set(0);
  new_queue.next_queue = NULL;
  new_queue.thread_array_id = -1;

  int i;
  for (i = 0; i < THREAD_MAX_THREADS; i++) {
    thread_array[i] = NULL;
  }
  thread_array[0] = (&core_thread);
  getcontext(&thread_array[0]->context);
  current = 0;
  interrupts_set(enabled);
}

Tid find_empty() {
  int enabled = interrupts_set(0);
  Tid i;
  for (i = 0; i < THREAD_MAX_THREADS; i++) {
    if (thread_array[i] == NULL) {
      interrupts_set(enabled);
      return i;
    }
  }
  interrupts_set(enabled);
  return THREAD_NOMORE;
}

void enqueue(Tid thread_index, struct ready_queue *rq) {
  int enabled = interrupts_set(0);
  struct ready_queue *current1 = rq;
  struct ready_queue *new_node =
      (struct ready_queue *)malloc(sizeof(struct ready_queue));
  new_node->thread_array_id = thread_index;
  new_node->next_queue = NULL;
  if (rq->next_queue == NULL) {
    rq->next_queue = new_node;
    interrupts_set(enabled);
    return;
  }
  while (current1->next_queue != NULL) {
    current1 = current1->next_queue;
  }
  current1->next_queue = new_node;
  interrupts_set(enabled);
  return;
}

Tid dequeue(struct ready_queue *rq) {
  int enabled = interrupts_set(0);
  struct ready_queue *temp = NULL;
  Tid temp_id;
  if (rq->next_queue == NULL) {
    interrupts_set(enabled);
    return THREAD_NONE;
  }
  temp = (rq->next_queue->next_queue);
  temp_id = rq->next_queue->thread_array_id;
  free(rq->next_queue);
  rq->next_queue = temp;
  interrupts_set(enabled);
  return temp_id;
}

Tid dequeue_by_index(Tid index, struct ready_queue *rq) {
  int enabled = interrupts_set(0);
  struct ready_queue *current1 = rq->next_queue;
  struct ready_queue *prev = NULL;
  Tid temp_id;
  if (rq->next_queue == NULL) {
    interrupts_set(enabled);
    return THREAD_NONE;
  }
  prev = rq->next_queue;
  current1 = rq->next_queue->next_queue;

  if (prev->thread_array_id == index) {
    rq->next_queue = current1;
    temp_id = prev->thread_array_id;
    free(prev);
    interrupts_set(enabled);
    return temp_id;
  }
  while (current1 != NULL) {
    if (current1->thread_array_id == index)
      break;
    prev = prev->next_queue;
    current1 = current1->next_queue;
  }
  if (current1 == NULL) {
    interrupts_set(enabled);
    return THREAD_INVALID;
  }
  prev->next_queue = current1->next_queue;
  temp_id = current1->thread_array_id;
  free(current1);
  interrupts_set(enabled);
  return temp_id;
}

Tid find_thread(Tid thread_number) {
  int enabled = interrupts_set(0);
  struct ready_queue *current1 = &new_queue;
  while (current1->next_queue != NULL) {
    if (current1->thread_array_id == thread_number) {
      interrupts_set(enabled);
      return current1->thread_array_id;
    }
    current1 = current1->next_queue;
  }
  if (current1 != NULL) {
    if (current1->thread_array_id == thread_number) {
      interrupts_set(enabled);
      return current1->thread_array_id;
    }
  }
  interrupts_set(enabled);
  return THREAD_INVALID;
}

Tid thread_id() {
  // TBD();
  return current;
}

void thread_stub(void (*thread_main)(void *), void *arg) {
  int enabled = interrupts_set(1);
  Tid ret;

  thread_main(arg); // call thread_main() function with arg
  interrupts_set(enabled);
  ret = thread_exit();
  // we should only get here if we are the last thread.
  assert(ret == THREAD_NONE);
  // all threads are done, so process should exit
  exit(0);
}

Tid thread_create(void (*fn)(void *), void *parg) {
  int enabled = interrupts_set(0);
  // TBD();
  int if_empty = find_empty();
  if (if_empty == THREAD_NOMORE) {
    interrupts_set(enabled);
    return THREAD_NOMORE;
  }
  struct thread *new_thread = (struct thread *)malloc(sizeof(struct thread));
  if (new_thread == NULL) {
    interrupts_set(enabled);
    return THREAD_NOMEMORY;
  }

  getcontext(&new_thread->context);

  new_thread->context.uc_stack.ss_sp = (char *)malloc(THREAD_MIN_STACK);
  new_thread->context.uc_stack.ss_size = THREAD_MIN_STACK;
  new_thread->context.uc_stack.ss_flags = 0;

  new_thread->context.uc_mcontext.gregs[REG_RIP] =
      (unsigned long int)thread_stub;

  new_thread->context.uc_mcontext.gregs[REG_RDI] = (unsigned long int)fn;
  new_thread->context.uc_mcontext.gregs[REG_RSI] = (unsigned long int)parg;

  unsigned long int sp;

  sp = (unsigned long int)(new_thread->context.uc_stack.ss_sp +
                           new_thread->context.uc_stack.ss_size);
  sp = sp - 1; // new_thread->context.uc_link

  // -16L = 0xFF FF FF FF - FF FF FF F0
  sp = (sp & -16L) - 8; // -8 leave space for interrupt signal
  new_thread->context.uc_mcontext.gregs[REG_RSP] = (unsigned long int)sp;

  enqueue(if_empty, &new_queue);
  thread_array[if_empty] = new_thread;
  thread_array[if_empty]->free_state = 0;
  interrupts_set(enabled);
  return if_empty;
}

Tid thread_yield(Tid want_tid) {
  int enabled = interrupts_set(0);
  // TBD();
  int trace;
  for (trace = 1; trace < THREAD_MAX_THREADS; trace++) {
    if (!thread_array[trace]) {
      continue;
    }
    if (thread_array[trace]->free_state == 1) {
      free(thread_array[trace]->context.uc_stack.ss_sp);
      free(thread_array[trace]);
      thread_array[trace] = NULL;
    }
  }
  int id;
  volatile int flag = 0;
  if (want_tid == THREAD_SELF) {
    interrupts_set(enabled);
    return current;
  } else if (want_tid == current) {
    interrupts_set(enabled);
    return current;
  } else if (want_tid == THREAD_ANY && new_queue.next_queue == NULL) {
    interrupts_set(enabled);
    return THREAD_NONE;
  } else if (find_thread(want_tid) == THREAD_INVALID) {
    interrupts_set(enabled);
    return THREAD_INVALID;
  } else {
    if (want_tid == THREAD_ANY) {
      id = dequeue(&new_queue);
      getcontext(&thread_array[current]->context);
      if (flag == 0) {
        flag = 1;
        enqueue(current, &new_queue);
        current = id;
        setcontext(&thread_array[id]->context);
      }
    } else {
      id = dequeue_by_index(want_tid, &new_queue);
      getcontext(&thread_array[current]->context);
      if (flag == 0) {
        flag = 1;
        enqueue(current, &new_queue);
        current = id;
        setcontext(&thread_array[id]->context);
      }
    }
  }
  interrupts_set(enabled);
  return id;
}

Tid thread_exit() {
  int enabled = interrupts_set(0);
  // TBD();
  if (new_queue.next_queue == NULL) {
    interrupts_set(enabled);
    return THREAD_NONE;
  }

  int next_thread = dequeue(&new_queue);
  if (next_thread == THREAD_NONE) {
    interrupts_set(enabled);
    return THREAD_NONE;
  }
  thread_array[current]->free_state = 1;
  current = next_thread;
  setcontext(&thread_array[next_thread]->context);
  interrupts_set(enabled);
  return THREAD_FAILED;
}

Tid thread_kill(Tid tid) {
  int enabled = interrupts_set(0);
  // TBD();
  if (find_thread(tid) == THREAD_INVALID || tid == current) {
    interrupts_set(enabled);
    return THREAD_INVALID;
  } else {
    dequeue_by_index(tid, &new_queue);
    free(thread_array[tid]->context.uc_stack.ss_sp);
    free(thread_array[tid]);
    thread_array[tid] = NULL;
    interrupts_set(enabled);
    return tid;
  }
  interrupts_set(enabled);
  return THREAD_FAILED;
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* This is the wait queue structure */
struct wait_queue {
  /* ... Fill this in ... */
  struct ready_queue wq;
};

struct wait_queue *wait_queue_create() {
  int enabled = interrupts_set(0);
  struct wait_queue *wq;
  wq = malloc(sizeof(struct wait_queue));
  assert(wq);
  // TBD();
  wq->wq.next_queue = NULL;
  wq->wq.thread_array_id = -1;
  interrupts_set(enabled);
  return wq;
}

void wait_queue_destroy(struct wait_queue *wq) {
  // TBD();
  free(wq);
}

Tid thread_sleep(struct wait_queue *queue) {
  // TBD();
  int enabled = interrupts_set(0);
  volatile int flag = 0;
  if (queue == NULL) {
    interrupts_set(enabled);
    return THREAD_INVALID;
  } else if (new_queue.next_queue == NULL) {
    interrupts_set(enabled);
    return THREAD_NONE;
  }
  int id = dequeue(&new_queue);
  getcontext(&thread_array[current]->context);
  if (flag == 0) {
    flag = 1;
    enqueue(current, &(queue->wq));
    current = id;
    setcontext(&thread_array[id]->context);
  }
  // enqueue(current, &(queue->wq));
  // int new_thread = dequeue(&new_queue);
  interrupts_set(enabled);
  return current;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int thread_wakeup(struct wait_queue *queue, int all) {
  // TBD();
  int enabled = interrupts_set(0);
  int count = 0;
  if (queue == NULL) {
    interrupts_set(enabled);
    return 0;
  } else if (queue->wq.next_queue == NULL) {
    interrupts_set(enabled);
    return 0;
  } else if (all == 0) {
    int waked_up = dequeue(&(queue->wq));
    enqueue(waked_up, &new_queue);
    interrupts_set(enabled);
    return 1;
  } else {
    while (queue->wq.next_queue != NULL) {
      int waked = dequeue(&(queue->wq));
      enqueue(waked, &new_queue);
      count++;
    }
    interrupts_set(enabled);
    return count;
  }
  interrupts_set(enabled);
  return 0;
}

struct lock {
  /* ... Fill this in ... */
  Tid lock_owner;
  struct wait_queue *wq;
};
struct lock *lock_create() {
  int enabled = interrupts_set(0);
  struct lock *lock;
  lock = malloc(sizeof(struct lock));
  assert(lock);
  lock->lock_owner = -1;
  lock->wq = wait_queue_create();
  // TBD();
  interrupts_set(enabled);
  return lock;
}

void lock_destroy(struct lock *lock) {
  int enabled = interrupts_set(0);
  assert(lock != NULL);
  // TBD();
  lock_acquire(lock);
  wait_queue_destroy(lock->wq);
  free(lock);
  interrupts_set(enabled);
}

void lock_acquire(struct lock *lock) {
  int enabled = interrupts_set(0);
  assert(lock != NULL);
  while (lock->lock_owner != -1 && lock->lock_owner != current) {
    thread_sleep(lock->wq);
  }
  lock->lock_owner = current;
  // TBD();
  interrupts_set(enabled);
}

void lock_release(struct lock *lock) {
  int enabled = interrupts_set(0);
  assert(lock != NULL);
  lock_acquire(lock);
  // TBD();
  thread_wakeup(lock->wq, 1);
  lock->lock_owner = -1;
  interrupts_set(enabled);
}

struct cv {
  /* ... Fill this in ... */
  struct wait_queue *wq;
};

struct cv *cv_create() {
  int enabled = interrupts_set(0);
  struct cv *cv;
  cv = malloc(sizeof(struct cv));
  assert(cv);
  // TBD();
  cv->wq = wait_queue_create();
  interrupts_set(enabled);
  return cv;
}

void cv_destroy(struct cv *cv) {
  int enabled = interrupts_set(0);
  assert(cv != NULL);

  // TBD();
  wait_queue_destroy(cv->wq);
  free(cv);
  interrupts_set(enabled);
}

void cv_wait(struct cv *cv, struct lock *lock) {
  int enabled = interrupts_set(0);
  assert(cv != NULL);
  assert(lock != NULL);
  lock_acquire(lock);
  lock_release(lock);
  thread_sleep(cv->wq);
  lock_acquire(lock);
  interrupts_set(enabled);
  // TBD();
}

void cv_signal(struct cv *cv, struct lock *lock) {
  int enabled = interrupts_set(0);
  assert(cv != NULL);
  assert(lock != NULL);
  lock_acquire(lock);
  thread_wakeup(cv->wq, 0);
  interrupts_set(enabled);
  // TBD();
}

void cv_broadcast(struct cv *cv, struct lock *lock) {
  int enabled = interrupts_set(0);
  assert(cv != NULL);
  assert(lock != NULL);
  lock_acquire(lock);
  thread_wakeup(cv->wq, 1);
  interrupts_set(enabled);
  // TBD();
}
