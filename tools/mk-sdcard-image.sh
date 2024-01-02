#! /bin/bash
#
# mk-sdcard-image.sh
# 
# Script to create a partitioned disk image that can be flashed onto an
# sd-card with dd, Balena Etcher or bmaptool.
#
# Requirements:
# mkfs.vfat
# mtools
# mke2fs
# sfdisk
#
# Work-in-progress:
# use mkfs.vfat and mtools (mcopy) to create FAT boot partition image
# and mke2fs to create and ext2 parition image. Concatenate these
# into a full disk image with a MBR sector created with sfdisk.
#
# Based on comments on Stack Overflow in:
# https://stackoverflow.com/questions/10949169/how-to-create-a-multi-partition-sd-disk-image-without-root-privileges/52850819#52850819
#
# TODO: Currently the generated ext2 partition image contains files with the UID/GID
# of the build machine's user, this is typically 1000/1000. This needs to be fixed so
# that files are generated with the root UID/GID of 0/0.  This should be possible by
# use of the 'psuedo' tool when building this project.
#

# Parameters that can be changed to build the image
img=sdimage.img
block_size=512
partition_size_1_megs=16
partition_size_2_megs=128
partition_file_1=part1_boot.fat
partition_file_2=part2_root.ext2
boot_dir=build/boot_partition
root_dir=build/host

# Calculate partition sizes in bytes
partition_size_1=$(($partition_size_1_megs * 1048576))
partition_size_2=$(($partition_size_2_megs * 1048576))


if [ ! -d "build/host" ]; then
  echo "Error: mk-sdimage.sh should only be run from build directory."
fi

#  Create kernel.img using tools/build-kernel-img.sh script
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
$SCRIPT_DIR/mk-kernel-img.sh

# Create the boot partition, FAT formatted.

echo "creating partition, sz= $partition_size_1"

rm -f "$partition_file_1"
dd if=/dev/zero of="$partition_file_1" bs=1M count=$partition_size_1_megs
mkfs.vfat -I -v "$partition_file_1"

# Use mcopy to copy files to this partition

for filename in $boot_dir/*; do
  echo "copying file: $filename to $partition_file_1"
  mcopy -i "$partition_file_1" $filename ::/
done

#exit 0

# Create second partition

echo "creating second partition"

rm -f "$partition_file_2"
mke2fs \
  -t ext2 \
  -b 1024 \
  -d "$root_dir" \
  -r 1 \
  -m 5 \
  -L 'cheviot' \
  "$partition_file_2" \
  "$((${partition_size_2_megs} * 1024))" \
;

echo "second partition created"
exit

# sfdisk defaults to aligning the first parition on a 1MB boundary
# If this changes add "start_" fields to specify partition start offsets
# in the partition table below
first_partition_offset=1048576
bs=1024

dd if=/dev/zero of="$img_file" bs="$bs" count=$((($first_partition_offset + $partition_size_1 + $partition_size_2)/$bs))

# Pass a partition table config to sfdisk to generate a blank image with the
# MBR parition table populated.
printf "
label: dos
unit: sectors
size=$(($partition_size_1/$block_size)), type=7, bootable
size=$(($partition_size_2/$block_size)), type=83
" | sfdisk "$img"

# Copy the boot and root partition images into the final disk image
cur_offset=$(($first_partition_offset))
dd if="$partition_file_1" of="$img" bs="$bs" seek="$(($cur_offset/$bs))"

cur_offset=$(($cur_offset + $partition_size_1))
dd if="$partition_file_2" of="$img" bs="$bs" seek="$(($cur_offset/$bs))"

sync

echo "Disk image generation complete"
echo "Full disk image file: $img"
echo "boot partition file : $partition_file_1"
echo "root partition file : $partition_file_2"

