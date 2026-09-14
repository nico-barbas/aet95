#include "core/rand.h"

static u32 pcg32_generator_rand_u32_impl(PCG32_Generator *data) {
  u64 old = data->state;
  data->state = old * 6364136223846793005 + data->inc;
  u32 xorshifted = (u32)(((old >> 18) ^ old) >> 27);
  u32 rot = (u32)(old >> 59);
  return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

static u32 pcg32_generator_rand_u32(Random_Generator generator) {
  PCG32_Generator *data = (PCG32_Generator *)generator.ptr;
  return pcg32_generator_rand_u32_impl(data);
}

static f32 pcg32_generator_rand_f32(Random_Generator generator) {
  PCG32_Generator *data = (PCG32_Generator *)generator.ptr;
  return (f32)(pcg32_generator_rand_u32_impl(data) >> 8) * 0x1.0p-24f;
}

void init_pcg32_generator(PCG32_Generator *data, u64 seed, u64 stream) {
  data->state = 0;
  data->inc = (stream << 1) | 1;
  pcg32_generator_rand_u32_impl(data);
  data->state += seed;
  pcg32_generator_rand_u32_impl(data);
}

Random_Generator pcg32_generator(PCG32_Generator *data) {
  return (Random_Generator){
    .ptr = data,
    .rand_u32_proc = pcg32_generator_rand_u32,
    .rand_f32_proc = pcg32_generator_rand_f32,
  };
}