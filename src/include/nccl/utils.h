/*************************************************************************
 * Copyright (c) 2016-2022, NVIDIA CORPORATION. All rights reserved.
 ************************************************************************/

#ifndef NCCL_UTILS_H_
#define NCCL_UTILS_H_

#include <stddef.h>
#include "types.h"

struct netIf {
  char prefix[64];
  int port;
};

int parseStringList(const char* string, struct netIf* ifList, int maxList);
int matchIfList(const char* string, int port, struct netIf* ifList, int listSize, int matchExact);

const char *get_plugin_lib_path();

#endif
