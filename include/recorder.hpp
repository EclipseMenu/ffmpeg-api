#pragma once

#include "render_settings.hpp"
#include "export.hpp"

#include <vector>
#include <string>
#include <memory>

class AVFormatContext;
class AVCodec;
class AVStream;
class AVCodecContext;
class AVBufferRef;
class AVFrame;
class AVPacket;

namespace ffmpeg {

class FFMPEG_API_DLL Recorder {
public:
    bool init(const RenderSettings& settings);
    void stop();

    bool writeFrame(const std::vector<uint8_t>& frameData);

    std::vector<std::string> getAvailableCodecs();

private:
    AVFormatContext* m_formatContext = nullptr;
    const AVCodec* m_codec = nullptr;
    AVStream* m_videoStream = nullptr;
    AVCodecContext* m_codecContext = nullptr;
    AVBufferRef* m_hwDevice = nullptr;
    AVFrame* m_frame = nullptr;
    AVPacket* m_packet;

    size_t m_frameCount = 0;
    bool m_init = false;
};

}