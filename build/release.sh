#!/bin/bash

#
#  ┌─────────────────────────────────────────────────────────────────────────────────────┐
#  │                                                                                     │
#  │   ████████╗██████╗  █████╗ ███╗   ███╗███████╗██╗     ███████╗ ██████╗ ███╗   ██╗   │
#  │   ╚══██╔══╝██╔══██╗██╔══██╗████╗ ████║██╔════╝██║     ██╔════╝██╔═══██╗████╗  ██║   │
#  │      ██║   ██████╔╝███████║██╔████╔██║█████╗  ██║     █████╗  ██║   ██║██╔██╗ ██║   │
#  │      ██║   ██╔══██╗██╔══██║██║╚██╔╝██║██╔══╝  ██║     ██╔══╝  ██║   ██║██║╚██╗██║   │
#  │      ██║   ██║  ██║██║  ██║██║ ╚═╝ ██║███████╗███████╗███████╗╚██████╔╝██║ ╚████║   │
#  │      ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝     ╚═╝╚══════╝╚══════╝╚══════╝ ╚═════╝ ╚═╝  ╚═══╝   │
#  │                                                                 www.trameleon.org   │
#  └─────────────────────────────────────────────────────────────────────────────────────┘
#

curr_dir="${PWD}"
build_dir="${curr_dir}/build"
base_dir="${curr_dir}/../"
install_dir="${base_dir}/install"
cmake_build_type="Release"
cmake_generator="Unix Makefiles"
build_type="release"
force_rebuild="OFF"

# ----------------------------------------------------

for var in "$@"
do
    if [ "${var}" = "debug" ] ; then
        cmake_build_type="Debug"
        build_type="debug"
        debug_flag="-debug"
    elif [ "${var}" = "rebuild" ] ; then
        force_rebuild="ON"
    fi
done

# ----------------------------------------------------

build_dir="${build_dir}.${build_type}"

if [ ! -d "${build_dir}" ] ; then
    mkdir "${build_dir}"
fi

# ----------------------------------------------------

cd "${build_dir}"

cmake -DCMAKE_INSTALL_PREFIX="${install_dir}" \
      -DCMAKE_BUILD_TYPE="${cmake_build_type}" \
      -DCMAKE_VERBOSE_MAKEFILE=ON \
      -DTRA_BUILD_STATIC_LIB=OFF \
      -DTRA_FORCE_REBUILD="${force_rebuild}" \
      -DTRA_NETINT_LIB_DIR="${curr_dir}/libxcoder/bin/" \
      -DTRA_NETINT_INC_DIR="${curr_dir}/libxcoder/source/" \
      -G "${cmake_generator}" \
      ..

if [ $? -ne 0 ] ; then
    echo "Failed to configure."
    exit 1
fi

cd "${build_dir}"
cmake --build . \
      --target install \
      --parallel $(nproc)

if [ $? -ne 0 ] ; then
    echo "Failed to build."
    exit 2
fi

# ----------------------------------------------------
