#pragma once

#include "export.hpp"

#include <Geode/Result.hpp>

#include <filesystem>

BEGIN_FFMPEG_NAMESPACE_V

class FFMPEG_API_DLL AudioMixer {
public:
    AudioMixer() = delete;
    AudioMixer(const AudioMixer&) = delete;
    AudioMixer(AudioMixer&&) = delete;

    /**
     * @brief Mixes a video file and an audio file into a single MP4 output.
     *
     * This function takes an input video file and an audio file, and merges them into a single MP4 output file. 
     * The output MP4 file will have both the video and audio streams synchronized.
     *
     * @param videoFile The path to the input video file.
     * @param audioFile The path to the input audio file.
     * @param outputMp4File The path where the output MP4 file will be saved.
     * 
     * @warning The audio file is expected to contain stereo (dual-channel) audio. Using other formats might lead to unexpected results.
     * @warning The video file is expected to contain a single video stream. Only the first video stream will be copied.
     */
    static geode::Result<> mixVideoAudio(std::filesystem::path videoFile, std::filesystem::path audioFile, std::filesystem::path outputMp4File);

    /**
     * @brief Mixes a video file and raw audio data into a single MP4 output.
     *
     * This function takes an input video file and raw audio data (in the form of a vector of floating-point samples),
     * and merges them into a single MP4 output file.
     *
     * @param videoFile The path to the input video file.
     * @param raw A vector containing the raw audio data (floating-point samples).
     * @param outputMp4File The path where the output MP4 file will be saved.
     *
     * @warning The raw audio data is expected to be stereo (dual-channel). Using mono or multi-channel audio might lead to issues.
     * @warning The video file is expected to contain a single video stream. Only the first video stream will be copied.
     */
    static geode::Result<> mixVideoRaw(const std::filesystem::path& videoFile, const std::vector<float>& raw, const std::filesystem::path &outputMp4File);
};

END_FFMPEG_NAMESPACE_V