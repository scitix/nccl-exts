/**
 * Copyright (c) 2024, Scitix Tech PTE. LTD. All rights reserved.
 *
 * See LICENSE file in the root directory of this source tree for terms.
 */

#include "monitor.h"
#include "options.h"

int main(int argc, char* argv[]) {
  ucommd::Options _;
  auto ret = _.parseArgs(argc, argv);
  if (ret) return ret;

  return ucommd::Monitor(_).run();
}
