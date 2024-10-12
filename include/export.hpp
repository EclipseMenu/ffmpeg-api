#pragma once

#include <Geode/platform/cplatform.h>

#ifdef GEODE_IS_WINDOWS
    #ifdef FFMPEG_API_EXPORTING
        #define FFMPEG_API_DLL __declspec(dllexport)
    #else
        #define FFMPEG_API_DLL __declspec(dllimport)
    #endif
#else
    #define FFMPEG_API_DLL __attribute__((visibility("default")))
#endif