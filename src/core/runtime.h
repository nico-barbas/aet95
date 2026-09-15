#ifndef CORE_RUNTIME_H
#define CORE_RUNTIME_H

#include "core/types.h"

#if defined(__STDC_DEFER_TS25755__)
#include <stddefer.h>
#else

typedef void (^Defer_Fn)(void);

static inline void run_defer_(Defer_Fn *fn) { (*fn)(); }

#define defer                                                                  \
  Defer_Fn concat_(defer_, __LINE__) __attribute__((cleanup(run_defer_))) = ^

typedef struct {
  Defer_Fn fn;
  bool8 *committed;
} Errdefer_;

static inline void run_errdefer_(Errdefer_ *e) {
  if (!*e->committed) {
    e->fn();
  }
}

#define errdefer_scope bool8 errdefer_committed_ = false
#define errdefer errdefer_impl_(concat_(errdefer_, __LINE__))
#define errdefer_impl_(name)                                                   \
  Errdefer_ name __attribute__((cleanup(run_errdefer_))) = {                   \
      .committed = &errdefer_committed_};                                      \
  (name).fn = ^

#define commit() (errdefer_committed_ = true)
#define return_ok(T, val)                                                      \
  do {                                                                         \
    commit();                                                                  \
    return ok(T, (val));                                                       \
  } while (0)

#endif

#endif