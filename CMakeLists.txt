cmake_minimum_required(VERSION 3.10)

set(APP_NAME vdec_test)
set(SDK_DIR /home/flork/T5_SDK/HD250_RTSP)
project(vdec_test)

SET (CMAKE_SYSTEM_NAME Linux)
SET (CMAKE_SYSTEM_PROCESSOR arm)
add_definitions(-std=c++11)
set(CMAKE_CXX_STANDARD 17)


set(MYSYSROOT ${SDK_DIR}/out/t507/demo2.0/longan/buildroot/host/aarch64-buildroot-linux-gnu/sysroot)
set(CMAKE_C_FLAGS "--sysroot=${MYSYSROOT} -mcpu=cortex-a53 -Wno-aligned-new  -fno-aggressive-loop-optimizations -fPIC -g -W -std=c++11  -DVERSION_GLES3  -DDEBU -D__ARM__ -DUSE_OZONE -DEGL_EGLEXT_PROTOTYPES -DGL_GLEXT_PROTOTYPES -s ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "--sysroot=${MYSYSROOT} -mcpu=cortex-a53 -Wno-aligned-new  -fno-aggressive-loop-optimizations -fPIC  -g -W -std=c++11  -DVERSION_GLES3  -DDEBU -D__ARM__ -DUSE_OZONE -DEGL_EGLEXT_PROTOTYPES -DGL_GLEXT_PROTOTYPES -s ${CMAKE_CXX_FLAGS}")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} --sysroot=${MYSYSROOT}" CACHE INTERNAL "" FORCE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --sysroot=${MYSYSROOT}" CACHE INTERNAL "" FORCE)
set(CMAKE_FIND_ROOT_PATH "${MYSYSROOT}")

SET(CMAKE_LINKER_FLAGS "-Wl,--gc-sections --sysroot=${MYSYSROOT}")
SET(CMAKE_C_COMPILER   ${SDK_DIR}/out/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc)
SET(CMAKE_CXX_COMPILER ${SDK_DIR}/out/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-g++)
SET(CMAKE_BUILD_TYPE Release CACHE STRING "default to Release" FORCE)



# add include file
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/X11
    ${CMAKE_CURRENT_SOURCE_DIR}/include/openGL
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cedarc_include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sdklib_include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ION_DRM
    ${CMAKE_CURRENT_SOURCE_DIR}/include/external
    ${CMAKE_CURRENT_SOURCE_DIR}/include/external/include/live
)

# add lib directories
link_directories(lib/lib64)
link_directories(lib/cedarx_lib)
link_directories(lib/openGL)

link_directories(
    ${SDK_DIR}/out/t507/demo2.0/longan/buildroot/target/usr/lib
    ${SDK_DIR}/out/t507/demo2.0/longan/buildroot/target/usr/lib/cedarc
    ${PROJECT_SOURCE_DIR}/libs/cedarx_lib
)
# add source file
aux_source_directory(./src SRC_LIST)


add_executable(${APP_NAME} ${SRC_LIST})

# add lib
target_link_libraries(${APP_NAME} EGL)
target_link_libraries(${APP_NAME} mali)
target_link_libraries(${APP_NAME} MemAdapter)
target_link_libraries(${APP_NAME} vdecoder)
target_link_libraries(${APP_NAME} VE)
target_link_libraries(${APP_NAME} cdc_base)
target_link_libraries(${APP_NAME} venc_codec)
target_link_libraries(${APP_NAME} venc_base)
target_link_libraries(${APP_NAME} sdk_memory)
target_link_libraries(${APP_NAME} cdx_ion)
target_link_libraries(${APP_NAME} sdk_decoder)
target_link_libraries(${APP_NAME} venc_codec)
target_link_libraries(${APP_NAME} pthread)
target_link_libraries(${APP_NAME} 
    ${PROJECT_SOURCE_DIR}/libs/libliveMedia.a
    ${PROJECT_SOURCE_DIR}/libs/libgroupsock.a
    ${PROJECT_SOURCE_DIR}/libs/libUsageEnvironment.a
    ${PROJECT_SOURCE_DIR}/libs/libBasicUsageEnvironment.a)
