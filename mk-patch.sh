#! /bin/sh

diff -aurN third_party_original/newlib-3.2.0 third_party/newlib-3.2.0 >patches/newlib-3.2.0.patch
diff -aurN third_party_original/binutils-2.25 third_party/binutils-2.25 >patches/binutils-2.25.patch
diff -aurN third_party_original/coreutils-5.2.1 third_party/coreutils-5.2.1 >patches/coreutils-5.2.1.patch
diff -aurN third_party_original/pdksh-5.2.12 third_party/pdksh-5.2.12 >patches/pdksh-5.2.12.patch
diff -aurN third_party_original/libtask third_party/libtask >patches/libtask.patch

