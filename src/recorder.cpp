#include "recorder.hpp"
#include "audio_mixer.hpp"

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
}

#include <iostream>

namespace ffmpeg {

std::vector<std::pair<int, std::string>> Recorder::getAvailableCodecs() {
    std::vector<std::pair<int, std::string>> vet;

    void* iter = nullptr;
    const AVCodec * codec;

    while ((codec = av_codec_iterate(&iter))) {
        if(codec->type == AVMEDIA_TYPE_VIDEO)
            vet.push_back({(int)codec->id, codec->name});
    }

    std::sort(vet.begin(), vet.end(), [](std::pair<int, std::string>& a, std::pair<int, std::string>& b) { return a.second < b.second; });
    
    return vet;
}

bool Recorder::init(const RenderSettings& settings) {
    avformat_alloc_output_context2(&m_formatContext, NULL, NULL, settings.m_outputFile.string().c_str());
    if (!m_formatContext) {
        geode::log::error("Could not create output context.");
        return false;
    }

    m_codec = avcodec_find_encoder((AVCodecID)settings.m_codecId);
    if (!m_codec) {
        geode::log::error("Could not find encoder.");
        return false;
    }

    m_videoStream = avformat_new_stream(m_formatContext, m_codec);
    if (!m_videoStream) {
        geode::log::error("Could not create video stream.");
        return false;
    }

    m_codecContext = avcodec_alloc_context3(m_codec);
    if (!m_codecContext) {
        geode::log::error("Could not allocate video codec context.");
        return false;
    }

    if(settings.m_hardwareAccelerationType != HardwareAccelerationType::NONE && av_hwdevice_ctx_create(&m_hwDevice, (AVHWDeviceType)settings.m_hardwareAccelerationType, NULL, NULL, 0) < 0) {
        geode::log::error("Could not create hardware device context.");
        return false;
    }

    m_codecContext->hw_device_ctx = m_hwDevice ? av_buffer_ref(m_hwDevice) : nullptr;
    m_codecContext->codec_id = m_codec->id;
    m_codecContext->bit_rate = settings.m_bitrate;
    m_codecContext->width = settings.m_width;
    m_codecContext->height = settings.m_height;
    m_codecContext->time_base = AVRational(1, settings.m_fps);
    m_codecContext->gop_size = 10;
    m_codecContext->max_b_frames = 1;
    m_codecContext->pix_fmt = AV_PIX_FMT_NONE;
    m_videoStream->time_base = m_codecContext->time_base;

    const AVPixelFormat *pix_fmt = m_codec->pix_fmts;
    if (pix_fmt) {
        while (*pix_fmt != AV_PIX_FMT_NONE) {
            if(*pix_fmt == (AVPixelFormat)settings.m_pixelFormat)
                m_codecContext->pix_fmt = *pix_fmt;
            ++pix_fmt;
        }
    }

    if(m_codecContext->pix_fmt == AV_PIX_FMT_NONE) {
        geode::log::info("Codec {} does not support pixel format, defaulting to codec's format", settings.m_codecId);
        m_codecContext->pix_fmt = m_codec->pix_fmts[0];
    }
    else
        geode::log::info("Codec {} supports pixel format.", settings.m_codecId);

    if (avcodec_open2(m_codecContext, m_codec, NULL) < 0) {
        geode::log::error("Could not open codec.");
        return false;
    }

    if (m_formatContext->oformat->flags & AVFMT_GLOBALHEADER)
        m_codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_parameters_from_context(m_videoStream->codecpar, m_codecContext) < 0) {
        geode::log::error("Could not copy codec parameters.");
        return false;
    }

    if (!(m_formatContext->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_formatContext->pb, settings.m_outputFile.string().c_str(), AVIO_FLAG_WRITE) < 0)
            return false;
    }

    if (avformat_write_header(m_formatContext, NULL) < 0) {
        geode::log::error("Could not write header.");
        return false;
    }

    m_swsCtx = sws_getContext(m_codecContext->width, m_codecContext->height, (AVPixelFormat)settings.m_pixelFormat, m_codecContext->width,
        m_codecContext->height, m_codecContext->pix_fmt, SWS_BILINEAR, NULL, NULL, NULL);

    if (!m_swsCtx) {
        geode::log::error("Could not create sws context.");
        return false;
    }

    m_frame = av_frame_alloc();
    m_frame->format = m_codecContext->pix_fmt;
    m_frame->width = m_codecContext->width;
    m_frame->height = m_codecContext->height;

    if (av_image_alloc(m_frame->data, m_frame->linesize, m_codecContext->width, m_codecContext->height, (AVPixelFormat)settings.m_pixelFormat, 32) < 0) {
        geode::log::error("Could not allocate raw picture buffer.");
        return false;
    }

    m_convertedFrame = av_frame_alloc();
    m_convertedFrame->format = m_codecContext->pix_fmt;
    m_convertedFrame->width = m_codecContext->width;
    m_convertedFrame->height = m_codecContext->height;
    if(av_image_alloc(m_convertedFrame->data, m_convertedFrame->linesize, m_convertedFrame->width, m_convertedFrame->height, m_codecContext->pix_fmt, 32) < 0) {
        geode::log::error("Could not allocate raw picture buffer.");
        return false;
    }

    m_packet = new AVPacket();

    av_init_packet(m_packet);
    m_packet->data = NULL;
    m_packet->size = 0;

    m_frameCount = 0;

    m_init = true;

    return true;
}

bool Recorder::writeFrame(const std::vector<uint8_t>& frameData) {
    if (!m_init || !m_frame || frameData.size() != m_frame->linesize[0] * m_frame->height)
        return false;

    const uint8_t* srcData[1] = { frameData.data() };
    int srcLinesize[1] = { m_frame->linesize[0] };

    sws_scale(
        m_swsCtx, srcData, srcLinesize, 0, m_frame->height,
        m_convertedFrame->data, m_convertedFrame->linesize
    );

    m_convertedFrame->pts = m_frameCount++;

    int ret = avcodec_send_frame(m_codecContext, m_convertedFrame);
    if (ret < 0)
        return false;

    while (ret >= 0) {
        ret = avcodec_receive_packet(m_codecContext, m_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        else if (ret < 0)
            return false;

        av_packet_rescale_ts(m_packet, m_codecContext->time_base, m_videoStream->time_base);
        m_packet->stream_index = m_videoStream->index;

        av_interleaved_write_frame(m_formatContext, m_packet);
        av_packet_unref(m_packet);
    }

    return true;
}

void Recorder::stop() {
    if(!m_init)
        return;

    m_init = false;

    avcodec_send_frame(m_codecContext, NULL);
    while (avcodec_receive_packet(m_codecContext, m_packet) == 0) {
        av_packet_rescale_ts(m_packet, m_codecContext->time_base, m_videoStream->time_base);
        m_packet->stream_index = m_videoStream->index;
        av_interleaved_write_frame(m_formatContext, m_packet);
        av_packet_unref(m_packet);
    }

    av_write_trailer(m_formatContext);

    avcodec_free_context(&m_codecContext);
    av_frame_free(&m_frame);
    av_frame_free(&m_convertedFrame);
    if (!(m_formatContext->oformat->flags & AVFMT_NOFILE)) {
        avio_close(m_formatContext->pb);
    }
    avformat_free_context(m_formatContext);

    delete m_packet;
}

}