#pragma once

#include "export.hpp"

#include <filesystem>

namespace ffmpeg {

class FFMPEG_API_DLL AudioMixer {
public:
    void mixMp4Wav(std::filesystem::path mp4File, std::filesystem::path wavFile, std::filesystem::path outputMp4File);
};

}