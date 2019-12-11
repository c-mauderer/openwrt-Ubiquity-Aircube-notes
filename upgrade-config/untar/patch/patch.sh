#!/bin/sh -x

set -e
set -u
set -x

# This script has to do a lot of things:
# - Patch the sysupgrade script in place. This is called from the web interface.
# - Copy and patch some scripts from /lib/upgrade. They are used by sysupgrade.
# - Copy and patch fwupdate.real (which is a link to ubntbox) to skip the
#   signature check. This is used by the patched /lib/upgrade scripts.

LIB_UPGRADE_PATCHED="/tmp/libupgrade"
UBNTBOX_PATCHED="/tmp/fwupdate.real"
UBNTBOX_SEEKOFFSET=24598

MD5FILE="/tmp/patchmd5"

cat <<EOF > ${MD5FILE}
54cb4e1ff4567a868f220960715938db  /sbin/sysupgrade
c33235322da5baca5a7b237c09bc8df1  /sbin/fwupdate.real
6fc82bd8a79cc02ec684de593db2901f  /lib/upgrade/platform.sh
EOF

# check md5 of files that will be patched
if ! md5sum -c ${MD5FILE}
then
	echo "******** Error when checking files. Refuse to do anything. ********"
	exit 0
fi

# prepare some overlay functionality
LOWERDIR="/tmp/root"
if [ ! -e ${LOWERDIR} ]
then
	mkdir -p ${LOWERDIR}
	mount -t squashfs -oro /dev/mtdblock3 ${LOWERDIR}
fi
overlay_some_path()
{
	PATH_TO_OVERLAY=$1
	ALIAS=$2
	UPPERDIR="/tmp/over_${ALIAS}"
	WORKDIR="/tmp/over_${ALIAS}_work"

	mkdir -p ${UPPERDIR}
	mkdir -p ${WORKDIR}

	mount -t overlay -o lowerdir=${LOWERDIR}${PATH_TO_OVERLAY},upperdir=${UPPERDIR},workdir=${WORKDIR} overlay ${PATH_TO_OVERLAY}
}

# Patch sysupgrade in place
overlay_some_path "/sbin" "sbin"
sed -i "s|include /lib/upgrade|include ${LIB_UPGRADE_PATCHED}|" /sbin/sysupgrade

# Copy and patch the ubntbox binary.
cp /sbin/fwupdate.real ${UBNTBOX_PATCHED}
echo -en '\x10' | dd of=${UBNTBOX_PATCHED} conv=notrunc bs=1 count=1 seek=24598

# Copy and patch the scripts
cp -r /lib/upgrade ${LIB_UPGRADE_PATCHED}
sed -i "s|echo \"/sbin/fwupdate.real\"|echo \"${UBNTBOX_PATCHED}\"|g" ${LIB_UPGRADE_PATCHED}/platform.sh

echo "******** Done ********"
