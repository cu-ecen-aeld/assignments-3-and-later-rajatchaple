#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

SHELLPATH=$(realpath $(dirname "$0"))


if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}
OUTPATH=$(realpath $OUTDIR)

echo "*********************************************************************************************"
echo $OUTPATH
# mkdir -p ${OUTPATH}


cd "$OUTPATH"


if [ ! -d "${OUTPATH}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTPATH}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTPATH}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
                            #make ARCH=${ARCH} menuconfig
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all 
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi 

echo "Adding the Image in OUTPATH"

echo "Creating the staging directory for the root filesystem"
echo "*********************************************************************************************"
echo $(pwd)
cd "$OUTPATH"
if [ -d "${OUTPATH}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTPATH}/rootfs and starting over"
    sudo rm  -rf ${OUTPATH}/rootfs
fi

#creating necessary base directory
mkdir ${OUTPATH}/rootfs
cd ${OUTPATH}/rootfs
cp ${OUTPATH}/linux-stable/arch/arm64/boot/Image ${OUTPATH}


mkdir bin dev etc home lib proc sbin sys tmp usr var
mkdir usr/bin usr/sbin usr/lib
mkdir -p var/log

echo "*********************************************************************************************"
echo $(pwd)
cd "$OUTPATH"
if [ ! -d "${OUTPATH}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} distclean
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
else
    cd busybox
fi


# TODO: Make and insatll busybox
echo "installing busybox"
make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=${OUTPATH}/rootfs install

cd ${OUTPATH}/rootfs

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"


# TODO: Add library dependencies to rootfs
export SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
echo $SYSROOT
mkdir lib64
cp -L $SYSROOT/lib/ld-linux-aarch64.* lib
cp -L $SYSROOT/lib64/libm.so.* lib64
cp -L $SYSROOT/lib64/libresolv.so.* lib64
cp -L $SYSROOT/lib64/libc.so.* lib64

# TODO: Make device nodes
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1

echo "Building writer utility"
# TODO: Clean and build the writer utility
cd $SHELLPATH && make clean
cd $SHELLPATH && make CROSS_COMPILE=${CROSS_COMPILE}


echo "Copying finder related scripts and executables in /home directory"
# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp ./writer ${OUTPATH}/rootfs/home
cp ./finder.sh ${OUTPATH}/rootfs/home
cp ./finder-test.sh ${OUTPATH}/rootfs/home
cp ./autorun-qemu.sh ${OUTPATH}/rootfs/home
cp -r ./conf/ ${OUTPATH}/rootfs/home

 

# TODO: Chown the root directory
echo "*********************************************************************************************"
echo $(pwd)
cd ${OUTPATH}/rootfs
sudo chown -R root:root *

echo "creating initramfs"
# TODO: Create initramfs.cpio.gz
find . | cpio -H newc -ov --owner root:root > ../initramfs.cpio
cd ..
gzip initramfs.cpio