// This file contains code for backwards compatibility with older ABI versions.

#include "audio_mixer.hpp"
#include "recorder.hpp"

namespace ffmpeg {
    typedef class FFMPEG_API_DLL AudioMixer {
    public:
        void mixVideoAudio(std::filesystem::path videoFile, std::filesystem::path audioFile, std::filesystem::path outputMp4File);
        void mixVideoRaw(std::filesystem::path videoFile, const std::vector<float>& raw, std::filesystem::path outputMp4File, uint32_t sampleRate);
        void mixVideoRaw(const std::filesystem::path& videoFile, const std::vector<float>& raw, const std::filesystem::path &outputMp4File);
    } AudioMixerV1;

    void AudioMixerV1::mixVideoAudio(std::filesystem::path videoFile, std::filesystem::path audioFile, std::filesystem::path outputMp4File) {
        (void) FFMPEG_API_VERSION_NS::AudioMixer::mixVideoAudio(std::move(videoFile), std::move(audioFile), std::move(outputMp4File));
    }

    void AudioMixerV1::mixVideoRaw(std::filesystem::path videoFile, const std::vector<float>& raw, std::filesystem::path outputMp4File, uint32_t sampleRate) {
        (void) FFMPEG_API_VERSION_NS::AudioMixer::mixVideoRaw(videoFile, raw, outputMp4File);
    }

    void AudioMixerV1::mixVideoRaw(const std::filesystem::path& videoFile, const std::vector<float>& raw, const std::filesystem::path &outputMp4File) {
        (void) FFMPEG_API_VERSION_NS::AudioMixer::mixVideoRaw(videoFile, raw, outputMp4File);
    }

    typedef struct RenderSettings {
        HardwareAccelerationType m_hardwareAccelerationType;
        PixelFormat m_pixelFormat;
        std::string m_codec;
        std::string m_colorspaceFilters;
        int64_t m_bitrate;
        uint32_t m_width;
        uint32_t m_height;
        uint16_t m_fps;
        std::filesystem::path m_outputFile;

        v2::RenderSettings toV2() const {
            return v2::RenderSettings {
                .m_hardwareAccelerationType = m_hardwareAccelerationType,
                .m_pixelFormat = m_pixelFormat,
                .m_codec = m_codec,
                .m_colorspaceFilters = m_colorspaceFilters,
                .m_doVerticalFlip = false,
                .m_bitrate = m_bitrate,
                .m_width = m_width,
                .m_height = m_height,
                .m_fps = m_fps,
                .m_outputFile = m_outputFile
            };
        }
    } RenderSettingsV1;

    typedef class FFMPEG_API_DLL Recorder {
    public:
        bool init(const RenderSettingsV1& settings);
        void stop();
        bool writeFrame(const std::vector<uint8_t>& frameData);
        std::vector<std::string> getAvailableCodecs();
    } RecorderV1;

#define self reinterpret_cast<FFMPEG_API_VERSION_NS::Recorder*>(this)
    bool RecorderV1::init(const RenderSettingsV1& settings) {
        auto res = self->init(settings.toV2());
        return res.isOk();
    }

    void RecorderV1::stop() {
        self->stop();
    }

    bool RecorderV1::writeFrame(const std::vector<uint8_t>& frameData) {
        auto res = self->writeFrame(frameData);
        return res.isOk();
    }

    std::vector<std::string> RecorderV1::getAvailableCodecs() {
        return FFMPEG_API_VERSION_NS::Recorder::getAvailableCodecs();
    }
#undef self

}