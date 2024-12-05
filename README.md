# SiCL Extensions for NCCL

SiCL (Scitix Communication Library kit) extension pack for AI includes a series
of plugins which enhance topology awareness and imporve networking performance
for [NVIDIA's NCCL](https://github.com/NVIDIA/nccl).

## Overview

[NCCL](https://developer.nvidia.com/nccl) provides the standard collective
communication routines for running distributed machine learning workloads
on top of NVIDIA GPUs. SiCL extensions implement net, tuner and profiler
plugins to allow NCCL-based applications to take full advantage of Scitix's
AI cluster design.

SiCL provides the following extensions for NCCL:

* unet: a NetPlugin with the support of net API v8/v7/v6/v5, enhances the
  awareness of GPU-RNIC associations (virtual inter-switch P2P links) on
  Scitix GPU server models, and embeds various counters (QPs, FIFO posts, etc.)
  for performance monitoring (e.g., rank-to-rank cross-node throughput) and
  problem diagnostics.

* utuner: internal algo + proto combination carefully tuned for a wide range
  of message sizes and GPU scales on Scitix platform, improving the overall
  performance for commonly used collective routines.

* uprofiler: to be released.

## Getting Started

To build the library, execute the `build.sh` script in the root directory of
this source tree. Primary build requirements include gcc, cmake (>=3.18), and
rdma-core (libibverbs).

Plugin objects (libnccl-net.so and libnccl-tuner.so) will be compiled and
generated in the `./build/` directory.

```shell
$ git clone https://github.com/scitix/nccl-exts.git
$ cd nccl-exts
$ ./build.sh
```

To enable SiCL extensions, add `./build/lib` to the library search path
(e.g., LD_LIBRARY_PATH) or copy the `.so` libs to your system library path.

```shell
# mkdir -p /usr/local/sihpc/lib
# cp ./build/lib/*.so /usr/local/sihpc/lib/.
# echo "/usr/local/sihpc/lib" > /etc/ld.so.conf.d/sihpc.conf
# ldconfig
```

## Pre-built Tarballs and Images

Another way to use the library is to install via release tarballs, which
have gone through more entensive testing. Following is reference docker build
commands of integrating SiCL exts to your container image.

```
RUN curl -o/tmp/sicl_install.sh https://oss-ap-southeast.scitix.ai/scitix-release/sicl-24.11-1.cuda1262.ubuntu2204.run && bash /tmp/sicl_install.sh && rm -rf /tmp/sicl_install.sh
ENV SICL_VERSION=24.11-1
```

Use the following pre-built images with SiCL installed for quick demonstration.

| Base Image | SiCL Pre-installed |
|------------|--------------------|
| nvcr.io/nvidia/pytorch:24.10-py3 | registry-ap-southeast.scitix.ai/hpc/ngc_pytorch:24.11-sicl |
| nvcr.io/nvidia/pytorch:24.06-py3 | registry-ap-southeast.scitix.ai/hpc/ngc_pytorch:24.06-sicl |
| nvcr.io/nvidia/nemo:24.07 | registry-ap-southeast.scitix.ai/hpc/nemo:24.07-sicl |

## Getting Help

If you have any issues in building or using the library, or if you think you may
have found a bug, please open an [issue](https://github.com/scitix/nccl-exts/issues).

## License

This library is licensed under the [Apache 2.0 License](LICENSE).
