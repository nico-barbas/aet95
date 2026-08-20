#ifndef CORE_RUNTIME_H
#define CORE_RUNTIME_H

#include "core/types.h"

// `defer { ... };` runs its body when the enclosing scope exits, on every path
// out: falling off the end, return, break, or a return from inside one of the
// statement-expression macros in types.h. Bodies run in reverse order of
// declaration. Jumping over a defer with goto is a hard compile error, not a
// warning.
//
// Two backends, one spelling. TS 25755 `defer` is used wherever the compiler
// has it; until then a clang block provides the same shape. The trailing
// semicolon is required by the block form and accepted by both, so call sites
// never change when the native path takes over.
#if defined(__STDC_DEFER_TS25755__)

#include <stddefer.h>

#else

typedef void (^Defer_Fn)(void);

// NOTE(nico): must stay `static inline`. A plain `static` trips
// -Wunused-function in any translation unit that includes this without
// deferring, which -Werror turns into a build failure.
static inline void run_defer_(Defer_Fn *fn) { (*fn)(); }

#define defer                                                                  \
  Defer_Fn concat_(defer_, __LINE__) __attribute__((cleanup(run_defer_))) = ^

#endif

#endif