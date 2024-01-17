#!/bin/bash
COMMON_DIR=$(cd `dirname $0`; pwd)
if [ -h $0 ]
then
        CMD=$(readlink $0)
        COMMON_DIR=$(dirname $CMD)
fi
cd $COMMON_DIR
cd ../../..
TOP_DIR=$(pwd)
COMMON_DIR=$TOP_DIR/device/rockchip/common
BOARD_CONFIG=$TOP_DIR/device/rockchip/.BoardConfig.mk
source $BOARD_CONFIG

if [ ! -n "$1" ];then
	echo "build all and save all as default"
	BUILD_TARGET=allsave
else
	BUILD_TARGET="$1"
	NEW_BOARD_CONFIG=$TOP_DIR/device/rockchip/$RK_TARGET_PRODUCT/$1
fi

usage()
{
	echo "====USAGE: build.sh modules===="
	echo "uboot              -build uboot"
	echo "kernel             -build kernel"
	echo "rootfs             -build default rootfs, currently build buildroot as default"
	echo "buildroot          -build buildroot rootfs"
	echo "ramboot            -build ramboot image"
	echo "sdcard             -build sdcard image"
	echo "yocto              -build yocto rootfs, currently build ros as default"
	echo "ros                -build ros rootfs"
	echo "debian             -build debian rootfs"
	echo "pcba               -build pcba"
	echo "recovery           -build recovery"
	echo "all                -build uboot, kernel, rootfs, recovery image"
	echo "cleanall           -clean uboot, kernel, rootfs, recovery"
	echo "firmware           -pack all the image we need to boot up system"
	echo "updateimg          -pack update image"
	echo "save               -save images, patches, commands used to debug"
	echo "default            -build all modules"
}

function build_uboot(){
	# build uboot
	echo "============Start build uboot============"
	echo "TARGET_UBOOT_CONFIG=$RK_UBOOT_DEFCONFIG"
	echo "========================================="
	if [ -f u-boot/*_loader_*.bin ]; then
		rm u-boot/*_loader_*.bin
	fi
	cd u-boot && ./make.sh $RK_UBOOT_DEFCONFIG && cd -
	if [ $? -eq 0 ]; then
		echo "====Build uboot ok!===="
	else
		echo "====Build uboot failed!===="
		exit 1
	fi
}

function build_sdcard(){
	if [ "x$RK_KERNEL_DTS" == "xrk3128-brk01" ];then
		SDCARD_SIZE=128
		date=$(date '+%Y%m%d')
		SDCARD_IMG="$date-$RK_KERNEL_DTS-sdcard-${SDCARD_SIZE}MB${FACTORY}.img"
		SDCARD_ZIP_IMG="$date-rk3128-brk01-sdcard-${SDCARD_SIZE}MB${FACTORY}.zip"
		DDR_BIN="../rkbin/bin/rk3128-sdcard/rk3128_ddr_300MHz_v2.09_uart0_20181106.bin"
		MINI_BIN="../rkbin/bin/rk3128-sdcard/rk312x_miniloader_v2.54_181106.bin"
		cd rockdev/
		dd if=/dev/zero of=$SDCARD_IMG bs=1M count=${SDCARD_SIZE}
		parted -s $SDCARD_IMG mklabel gpt
		parted -s $SDCARD_IMG mkpart uboot      4M 8M
		parted -s $SDCARD_IMG mkpart trust      8M 12M
		parted -s $SDCARD_IMG mkpart boot      12M 20M
		parted -s $SDCARD_IMG mkpart rootfs    20M 60M
		parted -s $SDCARD_IMG mkpart userdata    60M ${SDCARD_SIZE}M
		partprobe
		sgdisk --partition-guid=4:614e0000-0000-4b53-8000-1d28000054a9 $SDCARD_IMG

		../u-boot/tools/mkimage -n rk3128 -T rksd -d $DDR_BIN ./idbloader-sdcard.img
		cat $MINI_BIN >> ./idbloader-sdcard.img

		LOOP=$(sudo losetup -f |sed 's/\/dev\///g')
		echo ${LOOP}
		if [[ "x${LOOP}" != "x" ]]; then
			sudo kpartx -av $SDCARD_IMG
			sleep 3
			sudo dd conv=nocreat if=./idbloader-sdcard.img of=/dev/${LOOP} seek=64
			sudo dd conv=nocreat if=./uboot.img of=/dev/mapper/${LOOP}p1
			sudo dd conv=nocreat if=./trust.img of=/dev/mapper/${LOOP}p2
			sudo dd conv=nocreat if=./boot.img of=/dev/mapper/${LOOP}p3
			sudo dd conv=nocreat if=./rootfs.img of=/dev/mapper/${LOOP}p4
			sudo dd conv=nocreat if=./userdata.img of=/dev/mapper/${LOOP}p5
			sudo kpartx -d $SDCARD_IMG
			zip -r $SDCARD_ZIP_IMG $SDCARD_IMG
			printf "\nSDCard Image : [$SDCARD_IMG]\n"
			printf "SDCard ZIP Image : [$SDCARD_ZIP_IMG]\n"
		else
			printf "\n No loop device can be used \n"
		fi
		cd ../
	else
		printf "\n Not Support SDCard image\n"
	fi
}

function build_kernel(){
	# build kernel
	echo "============Start build kernel============"
	echo "TARGET_ARCH          =$RK_ARCH"
	echo "TARGET_KERNEL_CONFIG =$RK_KERNEL_DEFCONFIG"
	echo "TARGET_KERNEL_DTS    =$RK_KERNEL_DTS"
	echo "=========================================="
	cd $TOP_DIR/kernel && make ARCH=$RK_ARCH $RK_KERNEL_DEFCONFIG && make ARCH=$RK_ARCH $RK_KERNEL_DTS.img -j$RK_JOBS && cd -
	if [ $? -eq 0 ]; then
		echo "====Build kernel ok!===="
	else
		echo "====Build kernel failed!===="
		exit 1
	fi
}

function build_buildroot(){
	# build buildroot
	echo "==========Start build buildroot=========="
	echo "TARGET_BUILDROOT_CONFIG=$RK_CFG_BUILDROOT"
	echo "========================================="
	/usr/bin/time -f "you take %E to build builroot" $COMMON_DIR/mk-buildroot.sh $BOARD_CONFIG
	if [ $? -eq 0 ]; then
		echo "====Build buildroot ok!===="
	else
		echo "====Build buildroot failed!===="
		exit 1
	fi
}

function build_ramboot(){
	# build ramboot image
        echo "=========Start build ramboot========="
        echo "TARGET_RAMBOOT_CONFIG=$RK_CFG_RAMBOOT"
        echo "====================================="
	/usr/bin/time -f "you take %E to build ramboot" $COMMON_DIR/mk-ramdisk.sh ramboot.img $RK_CFG_RAMBOOT
	if [ $? -eq 0 ]; then
		echo "====Build ramboot ok!===="
	else
		echo "====Build ramboot failed!===="
		exit 1
	fi
}

function build_rootfs(){
	build_buildroot
}

function build_ros(){
	build_buildroot
}

function build_yocto(){
	echo "we don't support yocto at this time"
}

function build_debian(){
        # build debian
        echo "====Start build debian===="
	echo "TARGET_ARCH          =$RK_ARCH"
        echo "RK_ENABLE_MODULE     =$RK_ENABLE_MODULE"
	/usr/bin/time -f "you take %E to build debian" $COMMON_DIR/mk-debian.sh $RK_ENABLE_MODULE
        if [ $? -eq 0 ]; then
                echo "====Build debian ok!===="
        else
                echo "====Build debian failed!===="
                exit 1
        fi
}

function build_recovery(){
	# build recovery
	echo "==========Start build recovery=========="
	echo "TARGET_RECOVERY_CONFIG=$RK_CFG_RECOVERY"
	echo "========================================"
	/usr/bin/time -f "you take %E to build recovery" $COMMON_DIR/mk-ramdisk.sh recovery.img $RK_CFG_RECOVERY
	if [ $? -eq 0 ]; then
		echo "====Build recovery ok!===="
	else
		echo "====Build recovery failed!===="
		exit 1
	fi
}

function build_pcba(){
	# build pcba
	echo "==========Start build pcba=========="
	echo "TARGET_PCBA_CONFIG=$RK_CFG_PCBA"
	echo "===================================="
	/usr/bin/time -f "you take %E to build pcba" $COMMON_DIR/mk-pcba.sh pcba.img $RK_CFG_PCBA
	if [ $? -eq 0 ]; then
		echo "====Build pcba ok!===="
	else
		echo "====Build pcba failed!===="
		exit 1
	fi
}

function build_all(){
	echo "============================================"
	echo "TARGET_ARCH=$RK_ARCH"
	echo "TARGET_PLATFORM=$RK_TARGET_PRODUCT"
	echo "TARGET_UBOOT_CONFIG=$RK_UBOOT_DEFCONFIG"
	echo "TARGET_KERNEL_CONFIG=$RK_KERNEL_DEFCONFIG"
	echo "TARGET_KERNEL_DTS=$RK_KERNEL_DTS"
	echo "TARGET_BUILDROOT_CONFIG=$RK_CFG_BUILDROOT"
	echo "TARGET_RECOVERY_CONFIG=$RK_CFG_RECOVERY"
	echo "TARGET_PCBA_CONFIG=$RK_CFG_PCBA"
	echo "TARGET_RAMBOOT_CONFIG=$RK_CFG_RAMBOOT"
	echo "============================================"
	build_uboot
	build_kernel
	build_rootfs
#	build_recovery
	build_ramboot
}

function clean_all(){
	echo "clean uboot, kernel, rootfs, recovery"
	cd $TOP_DIR/u-boot/ && make distclean && cd -
	cd $TOP_DIR/kernel && make distclean && cd -
	rm -rf buildroot/out
}

function build_firmware(){
	HOST_DIR=$TOP_DIR/buildroot/output/host
	if [ -d "$TARGET_OUTPUT_DIR" ];then
		HOST_DIR=$TARGET_OUTPUT_DIR/host
	fi

	HOST_PATH=$HOST_DIR/usr/sbin:$HOST_DIR/usr/bin:$HOST_DIR/sbin:$HOST_DIR/bin

	# mkfirmware.sh to genarate image
	PATH=$HOST_PATH:$PATH ./mkfirmware.sh $BOARD_CONFIG
	if [ $? -eq 0 ]; then
	    echo "Make image ok!"
	else
	    echo "Make image failed!"
	    exit 1
	fi
}

function build_updateimg(){
	IMAGE_PATH=$TOP_DIR/rockdev
	PACK_TOOL_DIR=$TOP_DIR/tools/linux/Linux_Pack_Firmware

	echo "Make update.img"
	cd $PACK_TOOL_DIR/rockdev && ./mkupdate.sh && cd -
	mv $PACK_TOOL_DIR/rockdev/update.img $IMAGE_PATH
	if [ $? -eq 0 ]; then
	   echo "Make update image ok!"
	else
	   echo "Make update image failed!"
	   exit 1
	fi
}

function build_save(){
	IMAGE_PATH=$TOP_DIR/rockdev
	DATE=$(date  +%Y%m%d.%H%M)
	STUB_PATH=Image/"$RK_KERNEL_DTS"_"$DATE"_RELEASE_TEST
	STUB_PATH="$(echo $STUB_PATH | tr '[:lower:]' '[:upper:]')"
	export STUB_PATH=$TOP_DIR/$STUB_PATH
	export STUB_PATCH_PATH=$STUB_PATH/PATCHES
	mkdir -p $STUB_PATH

	#Generate patches
	#$TOP_DIR/.repo/repo/repo forall -c "$TOP_DIR/device/rockchip/common/gen_patches_body.sh"

	#Copy stubs
	#$TOP_DIR/.repo/repo/repo manifest -r -o $STUB_PATH/manifest_${DATE}.xml
	mkdir -p $STUB_PATCH_PATH/kernel
	cp $TOP_DIR/kernel/.config $STUB_PATCH_PATH/kernel
	cp $TOP_DIR/kernel/vmlinux $STUB_PATCH_PATH/kernel
	mkdir -p $STUB_PATH/IMAGES/
	cp $IMAGE_PATH/* $STUB_PATH/IMAGES/

	#Save build command info
	echo "UBOOT:  defconfig: $RK_UBOOT_DEFCONFIG" >> $STUB_PATH/build_cmd_info
	echo "KERNEL: defconfig: $RK_KERNEL_DEFCONFIG, dts: $RK_KERNEL_DTS" >> $STUB_PATH/build_cmd_info
	echo "BUILDROOT: $RK_CFG_BUILDROOT" >> $STUB_PATH/build_cmd_info

}

function build_all_save(){
	build_all
	build_firmware
	build_updateimg
	build_save
	build_sdcard
}
#=========================
# build target
#=========================
if [ $BUILD_TARGET == uboot ];then
    build_uboot
    exit 0
elif [ $BUILD_TARGET == kernel ];then
    build_kernel
    exit 0
elif [ $BUILD_TARGET == rootfs ];then
    build_rootfs
    exit 0
elif [ $BUILD_TARGET == buildroot ];then
    build_buildroot
    exit 0
elif [ $BUILD_TARGET == recovery ];then
    build_recovery
    exit 0
elif [ $BUILD_TARGET == ramboot ];then
    build_ramboot
    exit 0
elif [ $BUILD_TARGET == pcba ];then
    build_pcba
    exit 0
elif [ $BUILD_TARGET == yocto ];then
    build_yocto
    exit 0
elif [ $BUILD_TARGET == ros ];then
    build_ros
    exit 0
elif [ $BUILD_TARGET == debian ];then
    build_debian
    exit 0
elif [ $BUILD_TARGET == updateimg ];then
    build_updateimg
    exit 0
elif [ $BUILD_TARGET == all ];then
    build_all
    exit 0
elif [ $BUILD_TARGET == firmware ];then
    build_firmware
    exit 0
elif [ $BUILD_TARGET == save ];then
    build_save
    exit 0
elif [ $BUILD_TARGET == cleanall ];then
    clean_all
    exit 0
elif [ $BUILD_TARGET == --help ] || [ $BUILD_TARGET == help ] || [ $BUILD_TARGET == -h ];then
    usage
    exit 0
elif [ $BUILD_TARGET == allsave ];then
    build_all_save
    exit 0
elif [ -f $NEW_BOARD_CONFIG ];then
    rm -f $BOARD_CONFIG
    ln -s $NEW_BOARD_CONFIG $BOARD_CONFIG
elif [ $BUILD_TARGET == sdcard ];then
    build_sdcard
    exit 0
else
    echo "Can't found build config, please check again"
    usage
    exit 1
fi
