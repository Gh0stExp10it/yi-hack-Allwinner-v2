#!/bin/bash

export CROSSPATH=/opt/yi/toolchain-sunxi-musl/toolchain/bin
export PATH=${PATH}:${CROSSPATH}

export TARGET=arm-openwrt-linux
export CROSS=arm-openwrt-linux
export BUILD=x86_64-pc-linux-gnu

export CROSSPREFIX=${CROSS}-

export STRIP=${CROSSPREFIX}strip
export CXX=${CROSSPREFIX}g++
export CC=${CROSSPREFIX}gcc
export LD=${CROSSPREFIX}ld
export AS=${CROSSPREFIX}as
export AR=${CROSSPREFIX}ar

SCRIPT_DIR=$(cd `dirname $0` && pwd)
cd $SCRIPT_DIR

cd jq-1.5 || exit 1

make clean
make -j $(nproc) || exit 1

mkdir -p ../_install/bin || exit 1

cp ./jq ../_install/bin || exit 1

$STRIP ../_install/bin/* || exit 1
