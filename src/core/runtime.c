#include "core/runtime.h"

// NOTE(nico): clang emits a reference to _NSConcreteStackBlock for every stack
// block it lays down. A block that never escapes its scope only ever stores
// that pointer, never dereferences it, so a definition is all the linker needs.
// `defer` is the only thing here that makes blocks and its bodies never escape,
// so nothing can reach the contents. At -O1 and above the block is lowered away
// entirely and the symbol is not referenced at all; this only carries -O0
// Debug builds.
//
// Apple platforms ship the real BlocksRuntime in libSystem, and defining our
// own there would interpose on it.
//
// This stays OUT of unity.c and compiles as its own translation unit. Defining
// this symbol ahead of a block in the same TU segfaults the clang 21 frontend
// that zig 0.16 bundles (clang 22 is fine). Keeping it in a TU that lays down
// no blocks sidesteps that; appending it to the end of the jumbo build also
// works but silently breaks again the moment a deferring file is added below
// it.
#if !defined(__STDC_DEFER_TS25755__) && !defined(__APPLE__)
// The name is fixed by the block ABI and the linker has to see it, so it can be
// neither renamed nor given internal linkage.
// NOLINTNEXTLINE(bugprone-reserved-identifier,misc-use-internal-linkage,cert-dcl37-c,cert-dcl51-cpp)
void *_NSConcreteStackBlock[32] = {0};
#endif