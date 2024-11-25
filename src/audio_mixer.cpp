#include "audio_mixer.hpp"

#include <iostream>
#include <utility>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswresample/swresample.h>
}

//https://gist.github.com/royshil/fff30890c7c19a4889f0a148101c9dff
std::vector<float> readAudioFile(const char *filename, int targetSampleRate, AVSampleFormat targetSampleFormat, AVCodecParameters* outCodecParams)
{
	AVFormatContext *formatContext = nullptr;
	if (avformat_open_input(&formatContext, filename, nullptr, nullptr) != 0) {
		geode::log::error("Error opening file");
		return {};
	}

	if (avformat_find_stream_info(formatContext, nullptr) < 0) {
		geode::log::error("Error finding stream information");
		return {};
	}

	int audioStreamIndex = -1;
	for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
		if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			audioStreamIndex = i;
			break;
		}
	}

	if (audioStreamIndex == -1) {
		geode::log::error("No audio stream found");
		return {};
	}

	AVCodecParameters *codecParams = formatContext->streams[audioStreamIndex]->codecpar;
	const AVCodec *codec = avcodec_find_decoder(codecParams->codec_id);
	if (!codec) {
		geode::log::error("Decoder not found");
		return {};
	}

	AVCodecContext *codecContext = avcodec_alloc_context3(codec);
	if (!codecContext) {
		geode::log::error("Failed to allocate codec context");
		return {};
	}

	if (avcodec_parameters_to_context(codecContext, codecParams) < 0) {
		geode::log::error("Failed to copy codec parameters to codec context");
		return {};
	}

	if (avcodec_open2(codecContext, codec, nullptr) < 0) {
		geode::log::error("Failed to open codec");
		return {};
	}

    *outCodecParams = *codecParams;

	AVFrame *frame = av_frame_alloc();
	AVPacket packet;

	AVChannelLayout ch_layout;
    av_channel_layout_from_string(&ch_layout, "2 channels");
	SwrContext *swr = nullptr;
	int ret;
	ret = swr_alloc_set_opts2(&swr, &ch_layout, targetSampleFormat, targetSampleRate,
				  &(codecContext->ch_layout), codecContext->sample_fmt,
				  codecContext->sample_rate, 0, nullptr);
	if (ret < 0) {
		char errbuf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
		geode::log::error("Failed to set up swr context: {}", errbuf);
		return {};
	}
	ret = swr_init(swr);
	if (ret < 0) {
		char errbuf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
		geode::log::error("Failed to initialize swr context: {}", errbuf);
		return {};
	}

	std::vector<float> audioFrames;

	float *convertBuffer[2];
    convertBuffer[0] = static_cast<float *>(av_malloc(4096 * sizeof(float)));
    convertBuffer[1] = static_cast<float *>(av_malloc(4096 * sizeof(float)));

    while (av_read_frame(formatContext, &packet) >= 0) {
        if (packet.stream_index == audioStreamIndex && avcodec_send_packet(codecContext, &packet) == 0) {
                while (avcodec_receive_frame(codecContext, frame) == 0) {
                    int ret = swr_convert(swr, reinterpret_cast<uint8_t **>(convertBuffer), 4096,
                                        frame->data, frame->nb_samples);
                    if (ret < 0) {
                        char errbuf[AV_ERROR_MAX_STRING_SIZE];
                        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
                        geode::log::error("Failed to convert audio frame: {}", errbuf);
                        return {};
                    }

                    for (int i = 0; i < ret; ++i) {
                        audioFrames.push_back(convertBuffer[0][i]);
                        audioFrames.push_back(convertBuffer[1][i]);
                    }
            }
        }
        av_packet_unref(&packet);
    }

    av_free(convertBuffer[0]);
    av_free(convertBuffer[1]);

	swr_free(&swr);
	av_frame_free(&frame);
	avcodec_free_context(&codecContext);
	avformat_close_input(&formatContext);

	return audioFrames;
}

namespace ffmpeg {
    void AudioMixer::mixVideoAudio(std::filesystem::path videoFile, std::filesystem::path audioFile, std::filesystem::path outputMp4File) {
        constexpr int frameSize = 1024;

        AVFormatContext* wavFormatContext = nullptr;
        if (avformat_open_input(&wavFormatContext, audioFile.string().c_str(), nullptr, nullptr) < 0) {
            geode::log::error("Could not open file.");
            return;
        }

        AVCodecParameters inputAudioParams{};

        std::vector<float> raw = readAudioFile(audioFile.string().c_str(), 44100, AV_SAMPLE_FMT_FLTP, &inputAudioParams);

        mixVideoRaw(std::move(videoFile), raw, std::move(outputMp4File), inputAudioParams.sample_rate);

        avformat_close_input(&wavFormatContext);
    }

    void AudioMixer::mixVideoRaw(std::filesystem::path videoFile, const std::vector<float>& raw, std::filesystem::path outputMp4File, uint32_t sampleRate) {
        const int frameSize = 1024;

        geode::log::debug("raw size {}", raw.size());
        
        AVFormatContext* videoFormatContext = nullptr;
        if (avformat_open_input(&videoFormatContext, videoFile.string().c_str(), nullptr, nullptr) < 0) {
            geode::log::error("Could not open MP4 file.");
            return;
        }

        AVFormatContext* outputFormatContext = nullptr;
        avformat_alloc_output_context2(&outputFormatContext, nullptr, nullptr, outputMp4File.string().c_str());
        if (!outputFormatContext) {
            geode::log::error("Could not create output context.");
            avformat_close_input(&videoFormatContext);
            return;
        }

        AVStream* outputVideoStream = avformat_new_stream(outputFormatContext, nullptr);
        if (!outputVideoStream) {
            geode::log::error("Failed to create video stream.");
            return;
        }

        int videoStreamIndex = -1;
        for (unsigned int i = 0; i < videoFormatContext->nb_streams; i++) {
            if (videoFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoStreamIndex = i;
                break;
            }
        }

        AVCodecParameters* inputVideoParams = videoFormatContext->streams[videoStreamIndex]->codecpar;
        avcodec_parameters_copy(outputVideoStream->codecpar, inputVideoParams);
        outputVideoStream->codecpar->codec_tag = 0;

        AVStream* outputAudioStream = avformat_new_stream(outputFormatContext, nullptr);
        if (!outputAudioStream) {
            geode::log::error("Failed to create audio stream.");
            return;
        }

        constexpr int channels = 2;

        outputAudioStream->codecpar->codec_tag = 0;
        outputAudioStream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
        outputAudioStream->codecpar->sample_rate = sampleRate;
        outputAudioStream->codecpar->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
        outputAudioStream->codecpar->codec_id = AV_CODEC_ID_AAC;
        outputAudioStream->codecpar->bit_rate = 128000;
        outputAudioStream->codecpar->bits_per_coded_sample = 16;
        outputAudioStream->codecpar->format = AVSampleFormat::AV_SAMPLE_FMT_FLTP;
        outputAudioStream->codecpar->frame_size = frameSize;
        
        outputFormatContext->audio_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
        outputFormatContext->audio_codec_id = AV_CODEC_ID_AAC;
        outputFormatContext->bit_rate = 128000;

        AVCodecParameters *videoCodecParams = outputAudioStream->codecpar;
	    const AVCodec *audioCodec = avcodec_find_encoder(videoCodecParams->codec_id);
        AVCodecContext *audio_codec_context_encoder = avcodec_alloc_context3(audioCodec);

        if (avcodec_parameters_to_context(audio_codec_context_encoder, videoCodecParams) < 0) {
            geode::log::error("Failed to copy codec parameters to codec context.");
            return;
        }

        audio_codec_context_encoder->sample_rate = sampleRate;
        audio_codec_context_encoder->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
        audio_codec_context_encoder->sample_fmt = AV_SAMPLE_FMT_FLTP;
        audio_codec_context_encoder->time_base = AVRational{1, static_cast<int>(sampleRate)};

        int ret = avcodec_open2(audio_codec_context_encoder, audioCodec, nullptr);
        if (ret < 0) {
            geode::log::error("Failed to open encoder.");
            return;
        }

        outputVideoStream->codecpar->codec_id = AV_CODEC_ID_H264;
        outputVideoStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        outputFormatContext->video_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        outputFormatContext->video_codec_id = AV_CODEC_ID_H264;

        if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&outputFormatContext->pb, outputMp4File.string().c_str(), AVIO_FLAG_WRITE) < 0) {
                geode::log::error("Could not open output file.");
                avformat_free_context(outputFormatContext);
                avformat_close_input(&videoFormatContext);
                return;
            }
        }

        if (avformat_write_header(outputFormatContext, nullptr) < 0) {
            geode::log::error("Could not write header to output file.");
            avio_closep(&outputFormatContext->pb);
            avformat_free_context(outputFormatContext);
            avformat_close_input(&videoFormatContext);
            return;
        }

        geode::log::debug("1 timebase {} {} {} {}", videoFormatContext->streams[videoStreamIndex]->time_base.num, videoFormatContext->streams[videoStreamIndex]->time_base.den, outputVideoStream->time_base.num, outputVideoStream->time_base.den);

        AVPacket packet;
        while (true) {
            if (av_read_frame(videoFormatContext, &packet) >= 0) {
                av_packet_rescale_ts(&packet, videoFormatContext->streams[videoStreamIndex]->time_base, outputVideoStream->time_base);
                packet.stream_index = 0;
                av_interleaved_write_frame(outputFormatContext, &packet);
                av_packet_unref(&packet);
            } else {
                break;
            }
        }

        int pts = 0;

        AVFrame* audioFrame = av_frame_alloc();
        if (!audioFrame) {
            geode::log::error("Could not allocate audio frame.");
            return;
        }

        audioFrame->format = AV_SAMPLE_FMT_FLTP;
        audioFrame->ch_layout = AV_CHANNEL_LAYOUT_STEREO;

        for (size_t i = 0; i < raw.size(); i += frameSize * channels) {
            int samplesToEncode = std::min(frameSize, static_cast<int>((raw.size() - i) / channels));

            audioFrame->nb_samples = samplesToEncode;
            audioFrame->pts = pts;

            pts += samplesToEncode;

            if (av_frame_get_buffer(audioFrame, 0) < 0) {
                geode::log::error("Could not allocate audio buffer.");
                continue;
            }

            for (int j = 0; j < samplesToEncode; ++j) {
                reinterpret_cast<float*>(audioFrame->data[0])[j] = raw[i + j * channels];
                reinterpret_cast<float*>(audioFrame->data[1])[j] = raw[i + j * channels + 1];
            }

            if (avcodec_send_frame(audio_codec_context_encoder, audioFrame) < 0) {
                geode::log::error("Error sending audio frame to encoder.");
                continue;
            }

            AVPacket audioPacket;
            av_init_packet(&audioPacket);
            audioPacket.data = nullptr;
            audioPacket.size = 0;

            geode::log::debug("2 timebase {} {}", audio_codec_context_encoder->time_base.num, audio_codec_context_encoder->time_base.den, outputAudioStream->time_base.num, outputAudioStream->time_base.den);

            while (true) {
                int ret = avcodec_receive_packet(audio_codec_context_encoder, &audioPacket);
                if (ret == 0) {
                    av_packet_rescale_ts(&audioPacket, audio_codec_context_encoder->time_base, outputAudioStream->time_base);
                    audioPacket.stream_index = 1;
                    av_interleaved_write_frame(outputFormatContext, &audioPacket);
                    av_packet_unref(&audioPacket);
                } else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;
                else {
                    geode::log::error("Error receiving audio packet.");
                    break;
                }
            }
        }

        av_frame_free(&audioFrame);

        av_write_trailer(outputFormatContext);

        if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&outputFormatContext->pb);
        }
        avformat_free_context(outputFormatContext);
        avformat_close_input(&videoFormatContext);
    }
}