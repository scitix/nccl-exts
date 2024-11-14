/*************************************************************************
 * Copyright (c) 2017-2022, NVIDIA CORPORATION. All rights reserved.
 ************************************************************************/

#ifndef NCCL_NET_H_
#define NCCL_NET_H_

#define NCCL_NET_HANDLE_MAXSIZE 128
// Maximum number of requests per comm object
#define NCCL_NET_MAX_REQUESTS 32

#define NCCL_PTR_HOST 0x1
#define NCCL_PTR_CUDA 0x2
#define NCCL_PTR_DMABUF 0x4

#include "types.h"
#include "net_v8.h"
#include "net_v7.h"
#include "net_v6.h"
#include "net_v5.h"

#endif
