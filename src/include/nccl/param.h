/*************************************************************************
 * Copyright (c) 2017-2022, NVIDIA CORPORATION. All rights reserved.
 ************************************************************************/

#ifndef NCCL_PARAM_H_
#define NCCL_PARAM_H_

#include <stdint.h>

void ncclLoadParam(char const* env, int64_t deftVal, int64_t uninitialized, int64_t* cache);

#define NCCL_PARAM(name, env, deftVal) \
  int64_t ncclParam##name() { \
    constexpr int64_t uninitialized = INT64_MIN; \
    static_assert(deftVal != uninitialized, "default value cannot be the uninitialized value."); \
    static int64_t cache = uninitialized; \
    if (__builtin_expect(__atomic_load_n(&cache, __ATOMIC_RELAXED) == INT64_MIN, false)) { \
      ncclLoadParam("NCCL_" env, deftVal, INT64_MIN, &cache); \
    } \
    return cache; \
  }

#define SICL_PARAM(name, env, deftVal) \
  int64_t siclParam##name() { \
    constexpr int64_t uninitialized = INT64_MIN; \
    static_assert(deftVal != uninitialized, "default value cannot be the uninitialized value."); \
    static int64_t cache = uninitialized; \
    if (__builtin_expect(__atomic_load_n(&cache, __ATOMIC_RELAXED) == INT64_MIN, false)) { \
      ncclLoadParam("SICL_" env, deftVal, INT64_MIN, &cache); \
    } \
    return cache; \
  }

#endif
