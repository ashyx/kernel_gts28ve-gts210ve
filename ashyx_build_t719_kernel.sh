#!/bin/bash

#BUILD SCRIPT BY ASHYX

#SETUP BUILD ENVIROMENT
export ARCH=arm64
export CROSS_COMPILE=/home/mark/Kernels/tool-chains/aarch64-linux-android-4.9/bin/aarch64-linux-android-

#MAKE DEFCONFIG
make VARIANT_DEFCONFIG=msm8976_sec_gts28velte_eur_defconfig msm8976_sec_defconfig SELINUX_DEFCONFIG=selinux_defconfig SELINUX_LOG_DEFCONFIG=selinux_log_defconfig
cp $(pwd)/arch/arm64/configs/ashyx_gts28velte_defconfig $(pwd)/.config

#SETUP MENU CONFIG(REMOVE # TO ENABLE)
#make nconfig

#GET CORE COUNT AND MAKE
CORE_COUNT=`grep processor /proc/cpuinfo|wc -l`
make -j$CORE_COUNT

#BUILD DTB
tools/dtbTool -s 2048 -o arch/arm64/boot/dt.img -p scripts/dtc/ arch/arm/boot/dts/