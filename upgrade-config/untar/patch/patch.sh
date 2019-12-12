#!/bin/sh -x

set -e
set -u
set -x

# This script has to do a lot of things:
# - Patch the sysupgrade script in place. This is called from the web interface.
# - Copy and patch some scripts from /lib/upgrade. They are used by sysupgrade.
# - Copy and patch fwupdate.real (which is a link to ubntbox) to skip the
#   signature check. This is used by the patched /lib/upgrade scripts.

UBNTBOX_PATCHED="/tmp/fwupdate.real"

MD5FILE="/tmp/patchmd5"

cat <<EOF > ${MD5FILE}
c33235322da5baca5a7b237c09bc8df1  /sbin/fwupdate.real
EOF

# check md5 of files that will be patched
if ! md5sum -c ${MD5FILE}
then
	echo "******** Error when checking files. Refuse to do anything. ********"
	exit 0
fi

# prepare some overlay functionality
LOWERDIR="/tmp/lower_root"
mkdir -p ${LOWERDIR}
mount -t squashfs -oro /dev/mtdblock3 ${LOWERDIR}
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

# patch the ubntbox binary.
overlay_some_path "/sbin" "sbin"
echo -en '\x10' | dd of=/sbin/fwupdate.real conv=notrunc bs=1 count=1 seek=24598

echo "******** Done ********"
