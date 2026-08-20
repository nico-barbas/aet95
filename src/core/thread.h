#ifndef CORE_THREAD_H
#define CORE_THREAD_H

#include "core/allocator.h"
#include "core/array.h"
#include "core/types.h"

#include <pthread.h>
#include <stdatomic.h>

typedef enum Thread_Error {
  Thread_Error_None,
  Thread_Error_Failed_To_Create_Pool,
  Thread_Error_Failed_To_Push_Task,
} Thread_Error;

typedef struct Thread_Pool Thread_Pool;
typedef struct Thread_Task Thread_Task;
typedef void (*Thread_Proc)(Thread_Task task);

struct Thread_Task {
  // NOTE(nico): either an allocator owned by the task or a thread-safe
  // allocator. Of which there is none available today
  Allocator allocator;
  Thread_Proc proc;
  rawptr data;
  i64 user_index;
};

typedef struct Thread {
  Thread_Pool *pool;
  pthread_t handle;
} Thread;

#define LIST_TYPE bool8
#define LIST_TYPE_NAME Bool_List
#define LIST_FUNCTION_PREFIX bool_list
#include "core/list.h"

struct Thread_Pool {
  Allocator allocator;
  atomic_bool running;

  Array(Thread) threads;
  pthread_mutex_t mutex;
  pthread_cond_t cond;

  Array(Thread_Task) queue;
  usize queue_offset;
  usize queue_len;

  Bool_List tasks_done;
};

Thread_Error thread_pool_init(
    Thread_Pool *pool, usize thread_cap, usize task_cap, Allocator allocator
);
void destroy_thread_pool(Thread_Pool *pool);

// NOTE(nico): The rest of the API is push task and abort task, join the pool,
// wait etc..
void thread_pool_start(Thread_Pool *pool);
void thread_pool_join(Thread_Pool *pool);
void thread_pool_pop_join(Thread_Pool *pool);
Thread_Error thread_pool_push_task(Thread_Pool *pool, Thread_Task task);

#endif