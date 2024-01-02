#! /bin/sh
# 
# mk-patch.sh
#
# A script to generate patches of third party software.
#
# Extract the archives of the original software into a directory labelled third_party_original
# The original sources (with patch changes) are extracted into the third_party folder by
# the build process.
#
# TODO: Improve mechanism to create patches
#

if [ ! -f "meta/kernel.cmake" ]; then
  echo "Error: mk-patch.sh should only be run from the root directory."
fi

mkdir -p third_party_original

tar xf build/output/src/newlib-4.1.0.tar.gz -C third_party_original/
tar xf build/output/src/binutils-2.25.tar.bz2 -C third_party_original/
tar xf build/output/src/coreutils-5.2.1.tar.bz2 -C third_party_original/
tar xf build/output/src/pdksh-5.2.12.tar.gz -C third_party_original/
tar xf build/output/src/libtask-20121021.tgz -C third_party_original/
tar xf build/output/src/less-590.tar.gz -C third_party_original/


diff -aurN third_party_original/newlib-4.1.0 third_party/newlib-4.1.0 >patches/newlib-4.1.0.patch
diff -aurN third_party_original/binutils-2.25 third_party/binutils-2.25 >patches/binutils-2.25.patch
diff -aurN third_party_original/coreutils-5.2.1 third_party/coreutils-5.2.1 >patches/coreutils-5.2.1.patch
diff -aurN third_party_original/pdksh-5.2.12 third_party/pdksh-5.2.12 >patches/pdksh-5.2.12.patch
diff -aurN third_party_original/libtask third_party/libtask >patches/libtask.patch
diff -aurN third_party_original/less-590 third_party/less-590 >patches/less-590.patch
