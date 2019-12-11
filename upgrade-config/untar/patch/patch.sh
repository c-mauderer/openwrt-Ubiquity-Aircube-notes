#!/bin/sh -x

set -e
set -u
set -x

UBNTBOX="/sbin/ubntbox"
MD5_ORG="c33235322da5baca5a7b237c09bc8df1"
MD5_PATCHED="994b91bdd47d0d4770f91487199af6e9"

# check md5
MD5=`md5sum ${UBNTBOX} | cut -f 1 -d " "`
if [ "${MD5}" == "${MD5_PATCHED}" ]
then
	echo "******** System already patched ********"
	exit 0
fi

if [ "${MD5}" != "${MD5_ORG}" ]
then
	echo "******** Unknown ubntbox binary. Don't do anything. ********"
	exit 0
fi

# add overlay for critical paths so we can patch the files
LOWERDIR="/tmp/root"
mkdir -p ${LOWERDIR}
mount -t squashfs -oro /dev/mtdblock3 ${LOWERDIR}

overlay_some_path()
{
	PATH_TO_OVERLAY=$1
	ALIAS=$2
	UPPERDIR="/tmp/overlay_${ALIAS}"
	WORKDIR="/tmp/overlay_${ALIAS}_work"

	mkdir -p ${UPPERDIR}
	mkdir -p ${WORKDIR}

	mount -t overlay -o lowerdir=${LOWERDIR}${PATH_TO_OVERLAY},upperdir=${UPPERDIR},workdir=${WORKDIR} overlay ${PATH_TO_OVERLAY}
}

overlay_some_path "/sbin" "sbin"

# Patch the binary
echo -en '\x10' | dd of=${UBNTBOX} conv=notrunc bs=1 count=1 seek=24598

echo "******** Done ********"
