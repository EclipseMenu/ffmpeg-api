// This file contains code for backwards compatibility with older ABI versions.

#include "audio_mixer.hpp"
#include "recorder.hpp"

namespace ffmpeg {
    class FFMPEG_API_DLL AudioMixer {
    public:
        void mixVideoAudio(std::filesystem::path videoFile, std::filesystem::path audioFile, std::filesystem::path outputMp4File) {
            (void) FFMPEG_API_VERSION_NS::AudioMixer::mixVideoAudio(std::move(videoFile), std::move(audioFile), std::move(outputMp4File));
        }

        void mixVideoRaw(std::filesystem::path videoFile, const std::vector<float>& raw, std::filesystem::path outputMp4File, uint32_t sampleRate) {
            (void) FFMPEG_API_VERSION_NS::AudioMixer::mixVideoRaw(videoFile, raw, outputMp4File);
        }

        void mixVideoRaw(const std::filesystem::path& videoFile, const std::vector<float>& raw, const std::filesystem::path &outputMp4File) {
            (void) FFMPEG_API_VERSION_NS::AudioMixer::mixVideoRaw(videoFile, raw, outputMp4File);
        }
    };

    class FFMPEG_API_DLL Recorder {
#define self reinterpret_cast<FFMPEG_API_VERSION_NS::Recorder*>(this)
    public:
        bool init(const RenderSettings& settings) {
            auto res = self->init(settings);
            return res.isOk();
        }

        void stop() {
            self->stop();
        }

        bool writeFrame(const std::vector<uint8_t>& frameData) {
            auto res = self->writeFrame(frameData);
            return res.isOk();
        }

        std::vector<std::string> getAvailableCodecs() {
            return FFMPEG_API_VERSION_NS::Recorder::getAvailableCodecs();
        }
#undef self
    };

}