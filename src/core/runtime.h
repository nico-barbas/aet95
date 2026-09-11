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

#endif

#endif