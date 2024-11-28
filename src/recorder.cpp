#include "recorder.hpp"
#include "utils.hpp"

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
    #include <libavfilter/avfilter.h>
    #include <libavfilter/buffersrc.h>
    #include <libavfilter/buffersink.h>
}

namespace ffmpeg {

std::vector<std::string> Recorder::getAvailableCodecs() {
    std::vector<std::string> vec;

    void* iter = nullptr;
    const AVCodec * codec;

    while ((codec = av_codec_iterate(&iter))) {
        if(codec->type == AVMEDIA_TYPE_VIDEO &&
                (codec->id == AV_CODEC_ID_H264 || codec->id == AV_CODEC_ID_HEVC || codec->id == AV_CODEC_ID_VP8 || codec->id == AV_CODEC_ID_VP9 || codec->id == AV_CODEC_ID_AV1 || codec->id == AV_CODEC_ID_MPEG4) &&
                avcodec_find_encoder_by_name(codec->name) != nullptr && codec->pix_fmts && std::ranges::find(vec, std::string(codec->name)) == vec.end())
            vec.emplace_back(codec->name);
    }
    
    return vec;
}

const AVCodec* getCodecByName(const std::string& name) {
    void* iter = nullptr;
    const AVCodec * codec;
    while ((codec = av_codec_iterate(&iter))) {
        if(codec->type == AVMEDIA_TYPE_VIDEO && std::string(codec->name) == name)
            return codec;
    }
    return nullptr;
}

geode::Result<void> Recorder::init(const RenderSettings& settings) {
    int ret = avformat_alloc_output_context2(&m_formatContext, NULL, NULL, settings.m_outputFile.string().c_str());
    if (!m_formatContext)
        return geode::Err("Could not create output context: " + utils::getErrorString(ret));

    m_codec = getCodecByName(settings.m_codec);
    if (!m_codec)
        return geode::Err("Could not find encoder.");

    m_videoStream = avformat_new_stream(m_formatContext, m_codec);
    if (!m_videoStream)
        return geode::Err("Could not create video stream.");

    m_codecContext = avcodec_alloc_context3(m_codec);
    if (!m_codecContext)
        return geode::Err("Could not allocate video codec context.");

    if(settings.m_hardwareAccelerationType != HardwareAccelerationType::NONE && (ret = av_hwdevice_ctx_create(&m_hwDevice, (AVHWDeviceType)settings.m_hardwareAccelerationType, NULL, NULL, 0)); ret < 0)
        return geode::Err("Could not create hardware device context: " + utils::getErrorString(ret));

    m_codecContext->hw_device_ctx = m_hwDevice ? av_buffer_ref(m_hwDevice) : nullptr;
    m_codecContext->codec_id = m_codec->id;
    m_codecContext->bit_rate = settings.m_bitrate;
    m_codecContext->width = settings.m_width;
    m_codecContext->height = settings.m_height;
    m_codecContext->time_base = AVRational{1, settings.m_fps};
    m_codecContext->pix_fmt = AV_PIX_FMT_NONE;
    m_videoStream->time_base = m_codecContext->time_base;

    if (const AVPixelFormat *pix_fmt = m_codec->pix_fmts) {
        while (*pix_fmt != AV_PIX_FMT_NONE) {
            if(*pix_fmt == static_cast<AVPixelFormat>(settings.m_pixelFormat))
                m_codecContext->pix_fmt = *pix_fmt;
            ++pix_fmt;
        }
    }

    if(m_codecContext->pix_fmt == AV_PIX_FMT_NONE) {
        geode::log::info("Codec {} does not support pixel format, defaulting to codec's format", settings.m_codec);
        m_codecContext->pix_fmt = m_codec->pix_fmts[0];
    }
    else
        geode::log::info("Codec {} supports pixel format.", settings.m_codec);

    if (ret = avcodec_open2(m_codecContext, m_codec, nullptr); ret < 0)
        return geode::Err("Could not open codec: " + utils::getErrorString(ret));

    if (m_formatContext->oformat->flags & AVFMT_GLOBALHEADER)
        m_codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (ret = avcodec_parameters_from_context(m_videoStream->codecpar, m_codecContext); ret < 0)
        return geode::Err("Could not copy codec parameters: " + utils::getErrorString(ret));

    if (!(m_formatContext->oformat->flags & AVFMT_NOFILE)) {
        if (ret = avio_open(&m_formatContext->pb, settings.m_outputFile.string().c_str(), AVIO_FLAG_WRITE); ret < 0)
            return geode::Err("Could not open output file: " + utils::getErrorString(ret));
    }

    if (ret = avformat_write_header(m_formatContext, nullptr); ret < 0)
        return geode::Err("Could not write header: " + utils::getErrorString(ret));

    m_frame = av_frame_alloc();
    m_frame->format = m_codecContext->pix_fmt;
    m_frame->width = m_codecContext->width;
    m_frame->height = m_codecContext->height;

    if (ret = av_image_alloc(m_frame->data, m_frame->linesize, m_codecContext->width, m_codecContext->height, (AVPixelFormat)settings.m_pixelFormat, 32); ret < 0)
        return geode::Err("Could not allocate raw picture buffer: " + utils::getErrorString(ret));

    m_convertedFrame = av_frame_alloc();
    m_convertedFrame->format = m_codecContext->pix_fmt;
    m_convertedFrame->width = m_codecContext->width;
    m_convertedFrame->height = m_codecContext->height;
    if(ret = av_image_alloc(m_convertedFrame->data, m_convertedFrame->linesize, m_convertedFrame->width, m_convertedFrame->height, m_codecContext->pix_fmt, 32); ret < 0)
        return geode::Err("Could not allocate raw picture buffer: " + utils::getErrorString(ret));

    m_filteredFrame = av_frame_alloc();

    m_packet = av_packet_alloc();

    m_packet->data = nullptr;
    m_packet->size = 0;

    int inputPixelFormat = (int)settings.m_pixelFormat;

    if(!settings.m_colorspaceFilters.empty()) {
        m_filterGraph = avfilter_graph_alloc();
        if (!m_filterGraph)
            return geode::Err("Could not allocate filter graph.");

        const AVFilter* buffersrc = avfilter_get_by_name("buffer");
        const AVFilter* buffersink = avfilter_get_by_name("buffersink");
        const AVFilter* colorspace = avfilter_get_by_name("colorspace");

        char args[512];
        snprintf(args, sizeof(args),
                "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                m_codecContext->width, m_codecContext->height, m_codecContext->pix_fmt,
                m_codecContext->time_base.num, m_codecContext->time_base.den,
                m_codecContext->sample_aspect_ratio.num, m_codecContext->sample_aspect_ratio.den);

        if(ret = avfilter_graph_create_filter(&m_buffersrcCtx, buffersrc, "in", args, nullptr, m_filterGraph); ret < 0) {
            avfilter_graph_free(&m_filterGraph);
            return geode::Err("Could not create input for filter graph: " + utils::getErrorString(ret));
        }

        if(ret = avfilter_graph_create_filter(&m_buffersinkCtx, buffersink, "out", nullptr, nullptr, m_filterGraph); ret < 0) {
            avfilter_graph_free(&m_filterGraph);
            return geode::Err("Could not create output for filter graph: " + utils::getErrorString(ret));
        }

        if(ret = avfilter_graph_create_filter(&m_colorspaceCtx, colorspace, "colorspace", settings.m_colorspaceFilters.c_str(), nullptr, m_filterGraph); ret < 0) {
            avfilter_graph_free(&m_filterGraph);
            return geode::Err("Could not create colorspace for filter graph: " + utils::getErrorString(ret));
        }

        if(ret = avfilter_link(m_buffersrcCtx, 0, m_colorspaceCtx, 0); ret < 0) {
            avfilter_graph_free(&m_filterGraph);
            return geode::Err("Could not link filters: " + utils::getErrorString(ret));
        }

        if(ret = avfilter_link(m_colorspaceCtx, 0, m_buffersinkCtx, 0); ret < 0) {
            avfilter_graph_free(&m_filterGraph);
            return geode::Err("Could not link filters: " + utils::getErrorString(ret));
        }

        if (ret = avfilter_graph_config(m_filterGraph, nullptr); ret < 0) {
            avfilter_graph_free(&m_filterGraph);
            return geode::Err("Could not configure filter graph: " + utils::getErrorString(ret));
        }

        inputPixelFormat = av_buffersink_get_format(m_buffersinkCtx);
    }

    m_swsCtx = sws_getContext(m_codecContext->width, m_codecContext->height, (AVPixelFormat)inputPixelFormat, m_codecContext->width,
        m_codecContext->height, m_codecContext->pix_fmt, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

    if (!m_swsCtx)
        return geode::Err("Could not create sws context.");

    m_frameCount = 0;

    m_init = true;

    return geode::Ok();
}

geode::Result<void> Recorder::writeFrame(const std::vector<uint8_t>& frameData) {
    if (!m_init || !m_frame)
        return geode::Err("Recorder is not initialized.");

    if(frameData.size() != m_frame->linesize[0] * m_frame->height)
        return geode::Err("Frame data size does not match expected dimensions.");

    if(m_buffersrcCtx) {
        std::memcpy(m_frame->data[0], frameData.data(), frameData.size());
        geode::Result<void> res = filterFrame(m_frame, m_filteredFrame);

        if(res.isErr())
            return res;

        sws_scale(
            m_swsCtx, m_filteredFrame->data, m_filteredFrame->linesize, 0, m_filteredFrame->height,
            m_convertedFrame->data, m_convertedFrame->linesize);
    }
    else {
        const uint8_t* srcData[1] = { frameData.data() };
        int srcLinesize[1] = { m_frame->linesize[0] };

        sws_scale(
            m_swsCtx, srcData, m_frame->linesize, 0, m_frame->height,
            m_convertedFrame->data, m_convertedFrame->linesize);
    }

    m_convertedFrame->pts = m_frameCount++;

    int ret = avcodec_send_frame(m_codecContext, m_convertedFrame);
    if (ret < 0)
        return geode::Err("Error while sending frame: " + utils::getErrorString(ret));

    while (ret >= 0) {
        ret = avcodec_receive_packet(m_codecContext, m_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        if (ret < 0)
            return geode::Err("Error while receiving packet: " + utils::getErrorString(ret));

        av_packet_rescale_ts(m_packet, m_codecContext->time_base, m_videoStream->time_base);
        m_packet->stream_index = m_videoStream->index;

        av_interleaved_write_frame(m_formatContext, m_packet);
        av_packet_unref(m_packet);
    }

    av_frame_unref(m_filteredFrame);

    return geode::Ok();
}

geode::Result<void> Recorder::filterFrame(AVFrame* inputFrame, AVFrame* outputFrame) {
    int ret = 0;
    if (ret = av_buffersrc_add_frame(m_buffersrcCtx, inputFrame); ret < 0) {
        avfilter_graph_free(&m_filterGraph);
        return geode::Err("Could not feed frame to filter graph: " + utils::getErrorString(ret));
    }

    if (ret = av_buffersink_get_frame(m_buffersinkCtx, outputFrame); ret < 0) {
        av_frame_unref(outputFrame);
        return geode::Err("Could not retrieve frame from filter graph: " + utils::getErrorString(ret));
    }

    return geode::Ok();
}

void Recorder::stop() {
    if(!m_init)
        return;

    m_init = false;

    avcodec_send_frame(m_codecContext, nullptr);
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

    if(m_filterGraph) {
        avfilter_graph_free(&m_filterGraph);
        av_frame_free(&m_filteredFrame);
    }

    if (m_hwDevice) {
        av_buffer_unref(&m_hwDevice);
    }

    av_packet_free(&m_packet);
}

}