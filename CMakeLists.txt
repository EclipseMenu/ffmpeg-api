cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

if (MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
endif()

project(ffmpeg-api VERSION 1.0.0)

add_library(${PROJECT_NAME} SHARED
    src/recorder.cpp
    src/audio_mixer.cpp
    # Add any extra C++ source files here
)

set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Final>:Final>")


if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

target_include_directories(${PROJECT_NAME} PRIVATE
    include
)

setup_geode_mod(${PROJECT_NAME})

if (WIN32)
    set(STATIC_LIBS 
        ${CMAKE_SOURCE_DIR}/lib/avcodec.lib
        ${CMAKE_SOURCE_DIR}/lib/avformat.lib
        ${CMAKE_SOURCE_DIR}/lib/avutil.lib
        ${CMAKE_SOURCE_DIR}/lib/avfilter.lib
        ${CMAKE_SOURCE_DIR}/lib/swresample.lib
        ${CMAKE_SOURCE_DIR}/lib/swscale.lib
        ${CMAKE_SOURCE_DIR}/lib/libx264.lib
        ${CMAKE_SOURCE_DIR}/lib/x265-static.lib
        ${CMAKE_SOURCE_DIR}/lib/aom.lib
        ${CMAKE_SOURCE_DIR}/lib/vpx.lib
        ${CMAKE_SOURCE_DIR}/lib/openh264.lib
        ${CMAKE_SOURCE_DIR}/lib/ws2_32.lib
        ${CMAKE_SOURCE_DIR}/lib/mfplat.lib
        ${CMAKE_SOURCE_DIR}/lib/mf.lib
        ${CMAKE_SOURCE_DIR}/lib/secur32.lib
        ${CMAKE_SOURCE_DIR}/lib/crypt32.lib
        ${CMAKE_SOURCE_DIR}/lib/mfuuid.lib
        ${CMAKE_SOURCE_DIR}/lib/bcrypt.lib
        ${CMAKE_SOURCE_DIR}/lib/strmiids.lib
    ) # windows
elseif (ANDROID) 
    if (ANDROID_ABI STREQUAL "arm64-v8a")
        set(STATIC_LIBS
            ${CMAKE_SOURCE_DIR}/lib/arm64-v8a/libavcodec.a
            ${CMAKE_SOURCE_DIR}/lib/arm64-v8a/libavformat.a
            ${CMAKE_SOURCE_DIR}/lib/arm64-v8a/libavutil.a
            ${CMAKE_SOURCE_DIR}/lib/arm64-v8a/libavfilter.a
            ${CMAKE_SOURCE_DIR}/lib/arm64-v8a/libswresample.a
            ${CMAKE_SOURCE_DIR}/lib/arm64-v8a/libswscale.a
            ${CMAKE_SOURCE_DIR}/lib/arm64-v8a/libaom.a
            ${CMAKE_SOURCE_DIR}/lib/arm64-v8a/libvpx.a
            ${CMAKE_SOURCE_DIR}/lib/arm64-v8a/libx264.a
            ${CMAKE_SOURCE_DIR}/lib/arm64-v8a/libx265.a
        ) # android
    elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
        add_definitions(-D__USE_FILE_OFFSET64)
        add_definitions(-DANDROID_PLATFORM=24)
        set(STATIC_LIBS
            ${CMAKE_SOURCE_DIR}/lib/armeabi-v7a/libavcodec.a
            ${CMAKE_SOURCE_DIR}/lib/armeabi-v7a/libavformat.a
            ${CMAKE_SOURCE_DIR}/lib/armeabi-v7a/libavutil.a
            ${CMAKE_SOURCE_DIR}/lib/armeabi-v7a/libavfilter.a
            ${CMAKE_SOURCE_DIR}/lib/armeabi-v7a/libswresample.a
            ${CMAKE_SOURCE_DIR}/lib/armeabi-v7a/libswscale.a
            ${CMAKE_SOURCE_DIR}/lib/armeabi-v7a/libaom.a
            ${CMAKE_SOURCE_DIR}/lib/armeabi-v7a/libvpx.a
            ${CMAKE_SOURCE_DIR}/lib/armeabi-v7a/libx264.a
            ${CMAKE_SOURCE_DIR}/lib/armeabi-v7a/libx265.a
        ) # android
    endif()
else ()
    find_package(BZIP2 REQUIRED)
    set(STATIC_LIBS 
        "-framework Security"
        ${CMAKE_SOURCE_DIR}/lib/libx264.a
        ${CMAKE_SOURCE_DIR}/lib/libavcodec.a
        ${CMAKE_SOURCE_DIR}/lib/libavformat.a
        ${CMAKE_SOURCE_DIR}/lib/libavutil.a
        ${CMAKE_SOURCE_DIR}/lib/libavfilter.a
        ${CMAKE_SOURCE_DIR}/lib/libswresample.a
        ${CMAKE_SOURCE_DIR}/lib/libswscale.a     
        ${BZIP2_LIBRARY}
        "-lm -lbz2 -lz -liconv -pthread -framework Security -framework VideoToolbox -framework CoreFoundation -framework CoreMedia -framework CoreVideo -framework CoreServices -framework CoreAudio -framework AVFoundation -framework CoreGraphics -framework AudioToolbox -framework Metal -framework CoreImage -framework AppKit"
    ) # macos

    if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64") 
        set(STATIC_LIBS
            ${STATIC_LIBS}
            ${CMAKE_SOURCE_DIR}/lib/liblzma.a
        )
    endif()
endif()
target_link_libraries(${PROJECT_NAME} ${STATIC_LIBS})

if (PROJECT_IS_TOP_LEVEL)
    target_compile_definitions(${PROJECT_NAME} PRIVATE FFMPEG_API_EXPORTING)
endif()