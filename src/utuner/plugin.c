/*************************************************************************
 * Copyright (c) 2015-2019, NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2024, Scitix Tech PTE. LTD. All rights reserved.
 *
 ************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tuner.h"

#define MIN(i, j) (((i) < (j)) ? (i) : (j))
#define MAX(i, j) (((i) > (j)) ? (i) : (j))

struct AlgoProto {
  int algo;
  int proto;
  size_t nbytes;
};

struct TunerConfig {
  size_t nranks;
  size_t nnodes;
  size_t nchoices;
  struct AlgoProto ap_choices[64];
};

static const size_t kTunerCfgNum = 8;
static const struct TunerConfig allreduce_tuner_cfgs[128] = {
  {
    .nranks = 16,
    .nnodes = 2,
    .nchoices = 2,
    .ap_choices = {
      { NCCL_ALGO_TREE,  NCCL_PROTO_LL128, 1<<20, },
      { NCCL_ALGO_TREE,  NCCL_PROTO_LL128, 1<<23, },
    }
  },
  {
    .nranks = 32,
    .nnodes = 4,
    .nchoices = 7,
    .ap_choices = {
      { NCCL_ALGO_TREE,  NCCL_PROTO_LL128,  1<<21, },
      { NCCL_ALGO_UNDEF, NCCL_PROTO_UNDEF,  1<<22, },
      { NCCL_ALGO_UNDEF, NCCL_PROTO_UNDEF,  1<<24, },
      { NCCL_ALGO_TREE,  NCCL_PROTO_LL128,  1<<25, },
      { NCCL_ALGO_UNDEF, NCCL_PROTO_UNDEF,  1<<26, },
      { NCCL_ALGO_UNDEF, NCCL_PROTO_UNDEF,  1<<27, },
      { NCCL_ALGO_RING,  NCCL_PROTO_SIMPLE, 1<<28, },
    }
  },
  {
    .nranks = 48,
    .nnodes = 6,
    .nchoices = 7,
    .ap_choices = {
      { NCCL_ALGO_TREE,  NCCL_PROTO_LL128,  1<<21, },
      { NCCL_ALGO_UNDEF, NCCL_PROTO_UNDEF,  1<<22, },
      { NCCL_ALGO_UNDEF, NCCL_PROTO_UNDEF,  1<<24, },
      { NCCL_ALGO_TREE,  NCCL_PROTO_LL128,  1<<25, },
      { NCCL_ALGO_RING,  NCCL_PROTO_LL128,  1<<26, },
      { NCCL_ALGO_RING,  NCCL_PROTO_LL128,  1<<27, },
      { NCCL_ALGO_RING,  NCCL_PROTO_SIMPLE, 1<<28, },
    }
  },
  {
    .nranks = 64,
    .nnodes = 8,
    .nchoices = 8,
    .ap_choices = {
      { NCCL_ALGO_TREE,  NCCL_PROTO_LL128,  1<<21, },
      { NCCL_ALGO_UNDEF, NCCL_PROTO_UNDEF,  1<<22, },
      { NCCL_ALGO_UNDEF, NCCL_PROTO_UNDEF,  1<<24, },
      { NCCL_ALGO_TREE,  NCCL_PROTO_LL128,  1<<25, },
      { NCCL_ALGO_TREE,  NCCL_PROTO_LL128,  1<<26, },
      { NCCL_ALGO_RING,  NCCL_PROTO_LL128,  1<<27, },
      { NCCL_ALGO_UNDEF, NCCL_PROTO_UNDEF,  1<<28, },
      { NCCL_ALGO_RING,  NCCL_PROTO_SIMPLE, 1<<29, },
    }
  },
  {
    .nranks = 80,
    .nnodes = 10,
    .nchoices = 4,
    .ap_choices = {
      { NCCL_ALGO_TREE,  NCCL_PROTO_LL128, 1<<25, },
      { NCCL_ALGO_TREE,  NCCL_PROTO_LL128, 1<<26, },
      { NCCL_ALGO_RING,  NCCL_PROTO_LL128, 1<<27, },
      { NCCL_ALGO_RING,  NCCL_PROTO_LL128, 1<<28, },
    }
  },
  {
    .nranks = 96,
    .nnodes = 12,
    .nchoices = 4,
    .ap_choices = {
      { NCCL_ALGO_TREE,  NCCL_PROTO_LL128, 1<<25, },
      { NCCL_ALGO_TREE,  NCCL_PROTO_LL128, 1<<26, },
      { NCCL_ALGO_RING,  NCCL_PROTO_LL128, 1<<27, },
      { NCCL_ALGO_RING,  NCCL_PROTO_LL128, 1<<28, },
    }
  },
  {
    .nranks = 112,
    .nnodes = 14,
    .nchoices = 5,
    .ap_choices = {
      { NCCL_ALGO_TREE,  NCCL_PROTO_LL128,  1<<25, },
      { NCCL_ALGO_TREE,  NCCL_PROTO_LL128,  1<<27, },
      { NCCL_ALGO_RING,  NCCL_PROTO_LL128,  1<<28, },
      { NCCL_ALGO_UNDEF, NCCL_PROTO_UNDEF,  1<<29, },
      { NCCL_ALGO_RING,  NCCL_PROTO_SIMPLE, 1<<30, },
    }
  },
  {
    .nranks = 128,
    .nnodes = 16,
    .nchoices = 5,
    .ap_choices = {
      { NCCL_ALGO_TREE,  NCCL_PROTO_LL128,  1<<26, },
      { NCCL_ALGO_TREE,  NCCL_PROTO_LL128,  1<<27, },
      { NCCL_ALGO_RING,  NCCL_PROTO_LL128,  1<<28, },
      { NCCL_ALGO_RING,  NCCL_PROTO_LL128,  1<<29, },
      { NCCL_ALGO_RING,  NCCL_PROTO_SIMPLE, 1<<30, },
    }
  },
};

static const struct TunerConfig reducescatter_tuner_cfgs[128] = {
  {
    .nranks = 16,
    .nnodes = 2,
    .nchoices = 1,
    .ap_choices = {
      { NCCL_ALGO_RING,  NCCL_PROTO_LL, 1<<20, },
    }
  },
  {
    .nranks = 32,
    .nnodes = 4,
    .nchoices = 2,
    .ap_choices = {
      { NCCL_ALGO_RING,  NCCL_PROTO_LL, 1<<20, },
      { NCCL_ALGO_RING,  NCCL_PROTO_LL, 1<<21, },
    }
  },
  {
    .nranks = 48,
    .nnodes = 6,
    .nchoices = 2,
    .ap_choices = {
      { NCCL_ALGO_RING,  NCCL_PROTO_LL, 1<<21, },
      { NCCL_ALGO_RING,  NCCL_PROTO_LL, 1<<22, },
    }
  },
  {
    .nranks = 64,
    .nnodes = 8,
    .nchoices = 2,
    .ap_choices = {
      { NCCL_ALGO_RING,  NCCL_PROTO_LL, 1<<21, },
      { NCCL_ALGO_RING,  NCCL_PROTO_LL, 1<<22, },
    }
  },
  {
    .nranks = 80,
    .nnodes = 10,
    .nchoices = 2,
    .ap_choices = {
      { NCCL_ALGO_RING,  NCCL_PROTO_LL, 1<<21, },
      { NCCL_ALGO_RING,  NCCL_PROTO_LL, 1<<22, },
    }
  },
  {
    .nranks = 96,
    .nnodes = 12,
    .nchoices = 1,
    .ap_choices = {
      { NCCL_ALGO_RING,  NCCL_PROTO_LL, 1<<22, },
    }
  },
  {
    .nranks = 112,
    .nnodes = 14,
    .nchoices = 0,
    .ap_choices = {}
  },
  {
    .nranks = 128,
    .nnodes = 16,
    .nchoices = 0,
    .ap_choices = {}
  },
};

static const struct TunerConfig allgather_tuner_cfgs[128] = {
  {
    .nranks = 16,
    .nnodes = 2,
    .nchoices = 1,
    .ap_choices = {
      { NCCL_ALGO_RING,  NCCL_PROTO_LL, 1<<20, },
    }
  },
  {
    .nranks = 32,
    .nnodes = 4,
    .nchoices = 2,
    .ap_choices = {
      { NCCL_ALGO_RING,  NCCL_PROTO_LL, 1<<20, },
      { NCCL_ALGO_RING,  NCCL_PROTO_LL, 1<<21, },
    }
  },
  {
    .nranks = 48,
    .nnodes = 6,
    .nchoices = 1,
    .ap_choices = {
      { NCCL_ALGO_RING,  NCCL_PROTO_LL, 1<<21, },
    }
  },
  {
    .nranks = 64,
    .nnodes = 8,
    .nchoices = 2,
    .ap_choices = {
      { NCCL_ALGO_RING,  NCCL_PROTO_LL, 1<<21, },
      { NCCL_ALGO_RING,  NCCL_PROTO_LL, 1<<22, },
    }
  },
  {
    .nranks = 80,
    .nnodes = 10,
    .nchoices = 2,
    .ap_choices = {
      { NCCL_ALGO_RING,  NCCL_PROTO_LL, 1<<21, },
      { NCCL_ALGO_RING,  NCCL_PROTO_LL, 1<<22, },
    }
  },
  {
    .nranks = 96,
    .nnodes = 12,
    .nchoices = 1,
    .ap_choices = {
      { NCCL_ALGO_RING,  NCCL_PROTO_LL, 1<<22, },
    }
  },
  {
    .nranks = 112,
    .nnodes = 14,
    .nchoices = 0,
    .ap_choices = {}
  },
  {
    .nranks = 128,
    .nnodes = 16,
    .nchoices = 0,
    .ap_choices = {}
  },
};

static int get_tuner_cfg_index(size_t nRanks, size_t nNodes) {
  for (int i = 0; i < kTunerCfgNum; i++) {
    if (allreduce_tuner_cfgs[i].nranks == nRanks && allreduce_tuner_cfgs[i].nnodes == nNodes) {
      return i;
    }
  }
  return -1;
}

static void get_algo_proto(int cfgIndex, ncclFunc_t collType, size_t nBytes, int* algo, int* proto) {
  *algo = *proto = NCCL_ALGO_UNDEF; /* = NCCL_PROTO_UNDEF */

  if (cfgIndex < 0 || cfgIndex >= kTunerCfgNum ) return;

  const struct TunerConfig* tuner = NULL;
  if (collType == ncclFuncAllReduce) tuner = &allreduce_tuner_cfgs[cfgIndex];
  else if (collType == ncclFuncAllGather) tuner = &allgather_tuner_cfgs[cfgIndex];
  else if (collType == ncclFuncReduceScatter) tuner = &reducescatter_tuner_cfgs[cfgIndex];
  else return;

  const size_t choice_num = tuner->nchoices;
  if (choice_num == 0) return;

  int i = 0;
  while (i < choice_num - 1 && tuner->ap_choices[i].nbytes < nBytes) i++;

  if (i == 0 && nBytes < tuner->ap_choices[0].nbytes * 3 / 4) return;
  if (i == choice_num - 1 &&
      nBytes > tuner->ap_choices[choice_num - 1].nbytes + MIN(tuner->ap_choices[choice_num - 1].nbytes / 2, 2<<27)) return;
  if (i > 0 && nBytes < (tuner->ap_choices[i-1].nbytes + tuner->ap_choices[i].nbytes) / 2) i--;
  *algo = tuner->ap_choices[i].algo;
  *proto = tuner->ap_choices[i].proto;
}

#define __hidden __attribute__ ((visibility("hidden")))

__hidden ncclResult_t pluginGetCollInfo_v3(void* context, ncclFunc_t collType, size_t nBytes,
                              int numPipeOps, float** collCostTable, int numAlgo, int numProto,
                              int* nChannels) {
  if (context == NULL || collCostTable == NULL || *collCostTable == NULL) return ncclInvalidArgument;
  int algo = NCCL_ALGO_UNDEF, proto = NCCL_PROTO_UNDEF;
  get_algo_proto(*((int*)context), collType, nBytes, &algo, &proto);
  // Update NCCL core generated cost table. Updated table will be evaluated by NCCL to pick the best algo/proto combo
  if (NCCL_ALGO_UNDEF < algo && algo < NCCL_NUM_ALGORITHMS && NCCL_PROTO_UNDEF < proto && proto < NCCL_NUM_PROTOCOLS) {
    float (*cost_table_ptr)[NCCL_NUM_PROTOCOLS] = (float (*)[NCCL_NUM_PROTOCOLS])collCostTable;
    if (cost_table_ptr[algo][proto] != NCCL_ALGO_PROTO_IGNORE) {
      cost_table_ptr[algo][proto] = 0.0;
    }
  }
  // *nChannels = 4;
  return ncclSuccess;
}

__hidden ncclResult_t pluginInit_v2(size_t nRanks, size_t nNodes, ncclDebugLogger_t logFunction, void **context) {
  *context = malloc(sizeof(int));
  *((int*)(*context)) = get_tuner_cfg_index(nRanks, nNodes);
  return ncclSuccess;
}

__hidden ncclResult_t pluginGetCollInfo_v2(void* context, ncclFunc_t collType, size_t nBytes,
                              int collNetSupport, int nvlsSupport, int numPipeOps,
                              int *algorithm, int *protocol, int* nChannels) {
  get_algo_proto(*((int*)context), collType, nBytes, algorithm, protocol);
  return ncclSuccess;
}

__hidden ncclResult_t pluginDestroy_v2(void* context) { free(context); return ncclSuccess; }

static int tuner_cfg_index = -1;

__hidden ncclResult_t pluginInit_v1(size_t nRanks, size_t nNodes, ncclDebugLogger_t logFunction) {
  static int n = 0;
  static size_t nranks = 0, nnodes = 0;
  if (n++ == 0) {
    nranks = nRanks;
    nnodes = nNodes;
    tuner_cfg_index = get_tuner_cfg_index(nRanks, nNodes);
  } else
  if (!(nranks == nRanks && nnodes == nNodes)) {
    // WARN("UTUNER : tuner v1 does not support multiple communicators, fall back to default");
    tuner_cfg_index = -1;
  }
  return ncclSuccess;
}

__hidden ncclResult_t pluginGetCollInfo_v1(ncclFunc_t collType, size_t nBytes,
                              int collNetSupport, int nvlsSupport, int numPipeOps,
                              int *algorithm, int *protocol, int* nChannels) {
  get_algo_proto(tuner_cfg_index, collType, nBytes, algorithm, protocol);
  return ncclSuccess;
}

__hidden ncclResult_t pluginDestroy_v1() { return ncclSuccess; }

#define PLUGIN_NAME "UTUNER"

const ncclTuner_v3_t ncclTunerPlugin_v3 = {
  .name = PLUGIN_NAME,
  .init = pluginInit_v2,
  .getCollInfo = pluginGetCollInfo_v3,
  .destroy = pluginDestroy_v2
};

const ncclTuner_v2_t ncclTunerPlugin_v2 = {
  .name = PLUGIN_NAME,
  .init = pluginInit_v2,
  .getCollInfo = pluginGetCollInfo_v2,
  .destroy = pluginDestroy_v2
};

const ncclTuner_v1_t ncclTunerPlugin_v1 = {
  .name = PLUGIN_NAME,
  .init = pluginInit_v1,
  .getCollInfo = pluginGetCollInfo_v1,
  .destroy = pluginDestroy_v1
};
