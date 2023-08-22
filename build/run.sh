#!/bin/bash

#
#
#  ████████╗██████╗  █████╗ ███╗   ███╗███████╗██╗     ███████╗ ██████╗ ███╗   ██╗
#  ╚══██╔══╝██╔══██╗██╔══██╗████╗ ████║██╔════╝██║     ██╔════╝██╔═══██╗████╗  ██║
#     ██║   ██████╔╝███████║██╔████╔██║█████╗  ██║     █████╗  ██║   ██║██╔██╗ ██║
#     ██║   ██╔══██╗██╔══██║██║╚██╔╝██║██╔══╝  ██║     ██╔══╝  ██║   ██║██║╚██╗██║
#     ██║   ██║  ██║██║  ██║██║ ╚═╝ ██║███████╗███████╗███████╗╚██████╔╝██║ ╚████║
#     ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝     ╚═╝╚══════╝╚══════╝╚══════╝ ╚═════╝ ╚═╝  ╚═══╝
#                                                                www.trameleon.org

# ----------------------------------------------------

d="${PWD}"
base_dir="${d}/../"
inst_dir="${base_dir}/install"
build_type="release"
debugger=""
debug_flag=""

# ----------------------------------------------------

./release.sh "$@"
if [ $? -ne 0 ] ; then
    echo "Failed to build."
    exit 1
fi

# ----------------------------------------------------

for var in "$@"
do
    if [ "${var}" = "debug" ] ; then
        debug_flag="-debug"
        build_type="debug"
        debugger="gdb -ex \"handle SIGINT print nostop pass\" --args  "        
    fi
done

# ----------------------------------------------------

cd "${inst_dir}/bin/"

# ----------------------------------------------------

export DISPLAY=":0.0"
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${inst_dir}/lib
#export LIBVA_DRIVER_NAME=iHD

# ----------------------------------------------------

#${debugger} ./test-compile${debug_flag}
#${debugger} ./test-log${debug_flag}
#${debugger} ./test-registry${debug_flag}
#${debugger} ./test-profiler${debug_flag}
#${debugger} ./test-modules${debug_flag}
#${debugger} ./test-module-x264-encoder${debug_flag}
#${debugger} ./test-module-vaapi-encoder${debug_flag}
#${debugger} ./test-module-vaapi-decoder${debug_flag}
#${debugger} ./test-module-vtbox-encoder${debug_flag}
#${debugger} ./test-module-vtbox-decoder${debug_flag}
#${debugger} ./test-module-nvidia-encoder${debug_flag}
#${debugger} ./test-module-nvidia-decoder${debug_flag}
#${debugger} ./test-module-nvidia-transcoder${debug_flag}
#${debugger} ./test-module-nvidia-graphics${debug_flag}
#${debugger} ./test-module-nvidia-converter${debug_flag}
#${debugger} ./test-easy-encoder${debug_flag}
#${debugger} ./test-easy-decoder${debug_flag}
#${debugger} ./test-easy-transcoder${debug_flag}
#nvprof ${debugger} ./test-module-nvidia-converter${debug_flag} && ffmpeg -s 960x540 -pix_fmt nv12 -i "converted_960x540_yuv420pUVI.yuv" -pix_fmt rgb24 -y resized_960x540_yuv420pUVI.png && sxiv resized_960x540_yuv420pUVI.png
#nvprof ${debugger} ./test-module-nvidia-converter${debug_flag} 
#${debugger} ./test-opengl${debug_flag}
#${debugger} ./test-opengl-converter${debug_flag}
sudo ${debugger} ./test-module-netint-encoder${debug_flag}
sudo ${debugger} ./test-module-netint-decoder${debug_flag}

# ----------------------------------------------------
