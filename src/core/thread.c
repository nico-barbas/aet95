#include "core/thread.h"

#include "core/array.h"

#include <pthread.h>

// FIXME(nico): Return an error if needed
static Thread_Task thread_pool_pop_task(Thread_Pool *pool) {
  // FIXME(nico): check for empty queue
  Thread_Task task = pool->queue.items[pool->queue_offset];
  pool->queue_offset = (pool->queue_offset + 1) % pool->queue.len;
  pool->queue_len -= 1;

  return task;
}

static void *internal_runner_proc(void *thread_ptr) {
  Thread *thread = (Thread *)thread_ptr;
  Thread_Pool *pool = thread->pool;

  while (true) {
    pthread_mutex_lock(&pool->mutex);
    while (atomic_load(&pool->running) && pool->queue_len == 0) {
      pthread_cond_wait(&pool->cond, &pool->mutex);
    }

    if (pool->queue_len == 0) {
      break;
    }

    Thread_Task task = thread_pool_pop_task(pool);
    pthread_mutex_unlock(&pool->mutex);

    task.proc(task);

    pthread_mutex_lock(&pool->mutex);
    // TODO(nico): pop a task
  }

  pthread_mutex_unlock(&pool->mutex);
  return nullptr;
}

static usize thread_pool_task_queue_space(Thread_Pool *pool) {
  return pool->queue.len - pool->queue_len;
}

Thread_Error thread_pool_init(
    Thread_Pool *pool, usize thread_cap, usize task_cap, Allocator allocator
) {
  if (pthread_mutex_init(&pool->mutex, nullptr) != 0) {
    return Thread_Error_Failed_To_Create_Pool;
  }

  if (pthread_cond_init(&pool->cond, nullptr) != 0) {
    pthread_mutex_destroy(&pool->mutex);
    return Thread_Error_Failed_To_Create_Pool;
  }

  // NOTE(nico): probably need an error defer for this exact reason
  pool->threads = make_array(pool->threads, thread_cap, allocator);
  if (pool->threads.items == nullptr) {
    return Thread_Error_Failed_To_Create_Pool;
  }

  pool->queue = make_array(pool->queue, task_cap, allocator);
  if (pool->queue.items == nullptr) {
    return Thread_Error_Failed_To_Create_Pool;
  }

  pool->tasks_done = or_return(
      make_bool_list(512, allocator), Thread_Error_Failed_To_Create_Pool
  );

  return Thread_Error_None;
}

void destroy_thread_pool(Thread_Pool *pool) {
  pool->running = false;
  for (usize i = 0; i < pool->threads.len; i += 1) {
    pthread_join(array_get(pool->threads, i).handle, nullptr);
  }

  delete_bool_list(&pool->tasks_done);
  delete_array(pool->queue);
  delete_array(pool->threads);
}

void thread_pool_start(Thread_Pool *pool) {
  atomic_store(&pool->running, 1);

  for (usize i = 0; i < pool->threads.len; i += 1) {
    Thread *thread = array_get_ptr(pool->threads, i);
    thread->pool = pool;
    pthread_create(&thread->handle, nullptr, internal_runner_proc, thread);
  }
}

Thread_Error thread_pool_push_task(Thread_Pool *pool, Thread_Task task) {
  pthread_mutex_lock(&pool->mutex);

  if (thread_pool_task_queue_space(pool) == 0) {
    pthread_mutex_unlock(&pool->mutex);
    return Thread_Error_Failed_To_Push_Task;
  }
  usize index = (pool->queue_offset + pool->queue_len) % pool->queue.len;
  pool->queue.items[index] = task;
  pool->queue_len += 1;

  pthread_cond_signal(&pool->cond);
  pthread_mutex_unlock(&pool->mutex);

  return Thread_Error_None;
}
