cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")

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
        ${CMAKE_SOURCE_DIR}/lib/swresample.lib
        ${CMAKE_SOURCE_DIR}/lib/swscale.lib
        ${CMAKE_SOURCE_DIR}/lib/libx264.lib
        ${CMAKE_SOURCE_DIR}/lib/x265-static.lib
        Ws2_32.lib mfplat.lib mf.lib secur32.lib crypt32.lib mfuuid.lib
        mf mfplat mfuuid bcrypt strmiids.lib
    ) # windows
elseif (ANDROID) 
    if (ANDROID_ABI STREQUAL "arm64-v8a")
        set(STATIC_LIBS
            ${CMAKE_SOURCE_DIR}/lib/arm64-v8a/libavcodec.a
            ${CMAKE_SOURCE_DIR}/lib/arm64-v8a/libavformat.a
            ${CMAKE_SOURCE_DIR}/lib/arm64-v8a/libavutil.a
            ${CMAKE_SOURCE_DIR}/lib/arm64-v8a/libswresample.a
            ${CMAKE_SOURCE_DIR}/lib/arm64-v8a/libswscale.a
        ) # android
    elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
        set(STATIC_LIBS
            ${CMAKE_SOURCE_DIR}/lib/armeabi-v7a/libavcodec.a
            ${CMAKE_SOURCE_DIR}/lib/armeabi-v7a/libavformat.a
            ${CMAKE_SOURCE_DIR}/lib/armeabi-v7a/libavutil.a
            ${CMAKE_SOURCE_DIR}/lib/armeabi-v7a/libswresample.a
            ${CMAKE_SOURCE_DIR}/lib/armeabi-v7a/libswscale.a
        ) # android
    endif()
else ()
    set(STATIC_LIBS ...) # macos
endif()
target_link_libraries(${PROJECT_NAME} ${STATIC_LIBS})

if (PROJECT_IS_TOP_LEVEL)
    target_compile_definitions(${PROJECT_NAME} PRIVATE FFMPEG_API_EXPORTING)
endif()