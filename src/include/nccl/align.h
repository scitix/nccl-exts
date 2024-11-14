/*************************************************************************
 * Copyright (c) 2015-2019, NVIDIA CORPORATION. All rights reserved.
 ************************************************************************/

#ifndef NCCL_ALIGN_H_
#define NCCL_ALIGN_H_

#define DIVUP(x, y) \
    (((x)+(y)-1)/(y))

#define ROUNDUP(x, y) \
    (DIVUP((x), (y))*(y))

#define ALIGN_POWER(x, y) \
    ((x) > (y) ? ROUNDUP(x, y) : ((y)/((y)/(x))))

#define ALIGN_SIZE(size, align) \
    size = ((size + (align) - 1) / (align)) * (align);

#endif
