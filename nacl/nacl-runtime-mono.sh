#!/bin/bash
# Copyright (c) 2009 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that be
# found in the LICENSE file.
#

# nacl-runtime-mono.sh
#
# usage:  nacl-runtime-mono.sh
#
# this script builds mono runtime for Native Client 
#

readonly MONO_TRUNK_NACL=$(pwd)

source common.sh
source nacl-common.sh

readonly PACKAGE_NAME=mono
readonly INSTALL_PATH=${MONO_TRUNK_NACL}/naclmono-${CROSS_ID}

USE_CROSS_BUILD=${USE_CROSS_BUILD:-0}
if [ ${USE_CROSS_BUILD} = '0' ]; then
  export NACL_INTERP_LOADER=$MONO_TRUNK_NACL/nacl_interp_loader_sdk.sh
fi

BUILD_DIR=build-nacl-${CROSS_ID}

CustomConfigureStep() {
  Banner "Configuring ${PACKAGE_NAME}"
  set +e
  if [ -f build-name/Makefile ]; then
    cd build-nacl
  fi
  make distclean
  cd ${MONO_TRUNK_NACL}
  set -e

  if [ ${USE_CROSS_BUILD} = '1' ]; then
    ./patch_configure.py ../configure
    ./patch_configure.py ../libgc/configure
    ./patch_configure.py ../eglib/configure

    if [ $TARGET_BITSIZE == "32" ]; then
      CONFIG_OPTS="--host=i686-nacl"
    else
      CONFIG_OPTS="--host=x86_64-nacl"
    fi
    CONFIG_OPTS+=" --disable-mcs-build --cache-file=config.cache"
  else
    if [ $TARGET_BITSIZE == "32" ]; then
      CONFIG_OPTS="--host=i686-pc-linux-gnu --build=i686-pc-linux-gnu --target=i686-pc-linux-gnu"
    else
      CONFIG_OPTS="--host=x86_64-pc-linux-gnu --build=x86_64-pc-linux-gnu --target=x86_64-pc-linux-gnu"
    fi
    # UGLY hack to allow dynamic linking
    sed -i -e s/elf_i386/elf_nacl/ -e s/elf_x86_64/elf64_nacl/ ../configure
    sed -i -e s/elf_i386/elf_nacl/ -e s/elf_x86_64/elf64_nacl/ ../libgc/configure
    sed -i -e s/elf_i386/elf_nacl/ -e s/elf_x86_64/elf64_nacl/ ../eglib/configure
  fi

  Remove ${BUILD_DIR}
  MakeDir ${BUILD_DIR}
  cp config.cache ${BUILD_DIR}
  cd ${BUILD_DIR}
  CC=${NACLCC} CXX=${NACLCXX} AR=${NACLAR} RANLIB=${NACLRANLIB} PKG_CONFIG_PATH=${NACL_SDK_USR_LIB}/pkgconfig LD="${NACLLD}" \
  PKG_CONFIG_LIBDIR=${NACL_SDK_USR_LIB} PATH=${NACL_BIN_PATH}:${PATH} LIBS="-lnacl_dyncode -lc -lg -lnosys -lnacl" \
  CFLAGS="-g -O2 -D_POSIX_PATH_MAX=256 -DPATH_MAX=256" ../../configure \
    ${CONFIG_OPTS} \
    --exec-prefix=${INSTALL_PATH} \
    --libdir=${INSTALL_PATH}/lib \
    --prefix=${INSTALL_PATH} \
    --program-prefix="" \
    --oldincludedir=${INSTALL_PATH}/include \
    --with-glib=embedded \
    --with-tls=pthread \
    --enable-threads=posix \
    --without-sigaltstack \
    --without-mmap \
    --with-gc=included \
    --enable-nacl-gc \
    --with-sgen=no \
    --enable-nls=no \
    --enable-nacl-codegen \
    --disable-system-aot \
    --enable-shared \
    --disable-parallel-mark \
    --with-static-mono=no

}

CustomInstallStep() {
  make install
}

CustomPackageInstall() {
  CustomConfigureStep
  DefaultBuildStep
  CustomInstallStep
}

CustomPackageInstall
exit 0
