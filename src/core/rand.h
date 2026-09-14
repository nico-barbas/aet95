#ifndef CORE_RAND_H
#define CORE_RAND_H

#include "core/types.h"

#define rand_u32(generator) ((generator).rand_u32_proc(generator))
#define rand_f32(generator) ((generator).rand_f32_proc(generator))

typedef struct Random_Generator Random_Generator;
struct Random_Generator {
  rawptr ptr;
  u32 (*rand_u32_proc)(Random_Generator generator);
  f32 (*rand_f32_proc)(Random_Generator generator);
};

typedef struct PCG32_Generator {
  u64 state;
  u64 inc;
} PCG32_Generator;

void init_pcg32_generator(PCG32_Generator *data, u64 seed, u64 stream);
Random_Generator pcg32_generator(PCG32_Generator *data);

#endif