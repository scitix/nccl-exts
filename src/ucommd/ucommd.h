/**
 * Copyright (c) 2024, Scitix Tech PTE. LTD. All rights reserved.
 *
 * See LICENSE file in the root directory of this source tree for terms.
 */

#pragma once

#include "stats.h"

namespace ucommd {

void tryGenVTopo();

enum {
  UNET_PID = 0, UNET_RANK,
  UNET_IB_CQ_COUNT, UNET_IB_QP_COUNT, UNET_IB_MR_COUNT,
  UNET_IB_CPL_COUNT, UNET_IB_CPL_ERR_COUNT,
  UNET_IB_FIFO_POST_COUNT, UNET_IB_FIFO_RECV_COUNT,
  UNET_IB_TX_BYTES,
};
int UNET_BW_POST_BYTES_BY_RANK(int rank);
int UNET_BW_CPL_BYTES_BY_RANK(int rank);

StatPtr getUnetIbStat();
StatPtr getUnetBwStat();

} // namespace ucommd
