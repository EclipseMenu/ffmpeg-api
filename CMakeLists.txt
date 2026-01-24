cmake_minimum_required(VERSION 3.13)

project(ffmpeg-api)

include(FetchContent)

FetchContent_Declare(
    Geode
    GIT_REPOSITORY https://github.com/geode-sdk/geode.git
    GIT_TAG main
)
FetchContent_MakeAvailable(Geode)

add_geode_mod(
    ffmpeg-api
    VERSION v1.2.2
    GD android
)

target_sources(ffmpeg-api PUBLIC 
    src/audio_mixer.cpp
    src/utils.cpp
)

# Kita hapus semua link ke library ffmpeg eksternal
setup_geode_mod(ffmpeg-api)
