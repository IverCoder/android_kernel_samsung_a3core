#!/bin/bash

export CROSS_COMPILE=$(pwd)/toolchain/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-
export ARCH=arm64
export CLANG_TOOL_PATH=$(pwd)/toolchain/clang/host/linux-x86/clang-r383902/bin/
export PATH=${CLANG_TOOL_PATH}:${PATH//"${CLANG_TOOL_PATH}:"}

export BSP_BUILD_FAMILY=sharkl3
export DTC_OVERLAY_TEST_EXT=$(pwd)/tools/mkdtimg/ufdt_apply_overlay
export DTC_OVERLAY_VTS_EXT=$(pwd)/tools/mkdtimg/ufdt_verify_overlay_host
export BSP_BUILD_ANDROID_OS=y

make -C $(pwd) O=$(pwd)/out BSP_BUILD_DT_OVERLAY=y CC=clang LD=ld.lld ARCH=arm64 CLANG_TRIPLE=aarch64-linux-gnu- a3core_eur_open_defconfig -j12
make -C $(pwd) O=$(pwd)/out BSP_BUILD_DT_OVERLAY=y CC=clang LD=ld.lld ARCH=arm64 CLANG_TRIPLE=aarch64-linux-gnu- -j12

cp out/arch/arm64/boot/Image $(pwd)/arch/arm64/boot/Image

BUILD_OUTPUT="$(pwd)/out/arch/arm64/boot/Image"
PUBLISH_DIR="$(pwd)/Publish"
cd $PUBLISH_DIR
rm boot.img -f
tar -xvf a3core-A032FXXS6CXE1.tar.xz
chmod +x magiskboot
./magiskboot unpack stock-boot.img
rm kernel -f
cp $BUILD_OUTPUT kernel
./magiskboot repack stock-boot.img boot.img
rm $PUBLISH_DIR/kernel && rm $PUBLISH_DIR/ramdisk.cpio && rm $PUBLISH_DIR/dtb && rm $PUBLISH_DIR/stock-boot.img
