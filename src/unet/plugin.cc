/*************************************************************************
 * Copyright (c) 2016-2024, NVIDIA CORPORATION. All rights reserved.
 ************************************************************************/

#include <stdlib.h>
#include <stdint.h>

#include "net.h"
#include "debug.h"
#include "utils.h"
#include "ucommd.h"

ncclDebugLogger_t pluginLogFunction = NULL;

typedef enum nccl_unet_plugin {
  NCCL_UNET_IBV,
  NCCL_UNET_UCX,
  NCCL_UNET_OFI,
  NCCL_UNET_LAST
} nccl_unet_plugin_t;

static nccl_unet_plugin_t unet_plugin = NCCL_UNET_LAST;

extern ncclNet_v8_t ibvPlugin_v8;
extern ncclNet_v7_t ibvPlugin_v7;
extern ncclNet_v6_t ibvPlugin_v6;
extern ncclNet_v5_t ibvPlugin_v5;

ncclResult_t plugin_init_v8(ncclDebugLogger_t logFunction);
ncclResult_t plugin_init_v7(ncclDebugLogger_t logFunction);
ncclResult_t plugin_init_v6(ncclDebugLogger_t logFunction);
ncclResult_t plugin_init_v5(ncclDebugLogger_t logFunction);

ncclNet_v8_t ncclNetPlugin_v8 = { "UNET", plugin_init_v8, };
ncclNet_v7_t ncclNetPlugin_v7 = { "UNET", plugin_init_v7, };
ncclNet_v6_t ncclNetPlugin_v6 = { "UNET", plugin_init_v6, };
ncclNet_v5_t ncclNetPlugin_v5 = { "UNET", plugin_init_v5, };

static inline void plugin_setup()
{
  unet_plugin = NCCL_UNET_IBV;
  switch (unet_plugin) {
  case NCCL_UNET_IBV:
  case NCCL_UNET_UCX:
  case NCCL_UNET_OFI:
  default:
    ncclNetPlugin_v8 = ibvPlugin_v8;
    ncclNetPlugin_v7 = ibvPlugin_v7;
    ncclNetPlugin_v6 = ibvPlugin_v6;
    ncclNetPlugin_v5 = ibvPlugin_v5;
    break;
  }
  ucommd::tryGenVTopo();
}

#define PLUGIN_INIT_IMPL(_ver_) \
ncclResult_t plugin_init_v##_ver_(ncclDebugLogger_t logFunction) { \
  plugin_setup(); \
  pluginLogFunction = logFunction; \
  const char* plugin_path = get_plugin_lib_path(); \
  INFO(NCCL_INIT|NCCL_NET, "UNET/%s Plugin v%s loaded : %s", \
      unet_plugin == NCCL_UNET_IBV ? "IBV" : \
      unet_plugin == NCCL_UNET_UCX ? "UCX" : \
      unet_plugin == NCCL_UNET_OFI ? "OFI" : \
      "Unexpected", \
      #_ver_, \
      plugin_path ? plugin_path : "unknown"); \
  return ncclNetPlugin_v##_ver_.init(logFunction); \
}
PLUGIN_INIT_IMPL(8)
PLUGIN_INIT_IMPL(7)
PLUGIN_INIT_IMPL(6)
PLUGIN_INIT_IMPL(5)
