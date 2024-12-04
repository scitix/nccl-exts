#!/usr/bin/env bash
#
# Copyright (c) 2024, Scitix Tech PTE. LTD. All rights reserved.
#
ROOT_DIR=$(cd $(dirname $0) && pwd)
BUILD_DIR=$ROOT_DIR/build

usage() {
  echo "  Usage: ./build.sh [OPTIONS]"
  echo
  echo "  OPTIONS:"
  echo "    --release         Build in release mode, default: ON"
  echo "    --debug           Build in debug mode, default: OFF"
  echo "    --verbose         Verbose build"
  echo

  exit 0
}

CMAKEARGS=""
ARGS=$(getopt -l \
release,\
debug,\
verbose,\
help -o h -- "$@") || usage

eval set -- "$ARGS"
while [ -n "$1" ]; do
  case "$1" in
  --release)
    CMAKEARGS+="-DCMAKE_BUILD_TYPE=Release "
    shift
    ;;
  --debug)
    CMAKEARGS+="-DCMAKE_BUILD_TYPE=Debug "
    shift
    ;;
  --verbose)
    CMAKEARGS+="-DCMAKE_VERBOSE_MAKEFILE=ON "
    shift
    ;;
  -h|--help)
    usage
    ;;
  --)
    shift
    break
    ;;
  esac
done

[ x$1 != "x" ] && usage

[ -z "$CMAKEARGS" ] || echo "CMake arguments: $CMAKEARGS"

rm -rf $BUILD_DIR && mkdir -p $BUILD_DIR
cmake $CMAKEARGS -B$BUILD_DIR -S$ROOT_DIR && make -C$BUILD_DIR -j8
if [ $? -ne 0 ]; then
  echo "failed to build SiCL extensions for NCCL ..."
  exit 1
else
  echo "SiCL extensions for NCCL: construction completed :-p"
  exit 0
fi
