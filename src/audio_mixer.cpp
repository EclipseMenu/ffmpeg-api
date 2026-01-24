#include "audio_mixer.hpp"
#include "utils.hpp"
#include <cstdlib>
#include <string>
#include <vector>
#include <filesystem>

BEGIN_FFMPEG_NAMESPACE_V

    geode::Result<> AudioMixer::mixVideoAudio(std::filesystem::path videoFile, std::filesystem::path audioFile, std::filesystem::path outputMp4File) {
        
        std::string cmd = "su -c '/data/data/com.termux/files/home/GDMPEG/dist/bin/gdmpeg_test \"" + 
                          videoFile.string() + "\" \"" + 
                          audioFile.string() + "\" \"" + 
                          outputMp4File.string() + "\"'";

        int result = std::system(cmd.c_str());

        if (result == 0) return geode::Ok();
        return geode::Err("GDMPEG Gagal.");
    }

    geode::Result<> AudioMixer::mixVideoRaw(const std::filesystem::path& videoFile, const std::vector<float>& raw, const std::filesystem::path &outputMp4File) {
        return geode::Ok();
    }

END_FFMPEG_NAMESPACE_V
