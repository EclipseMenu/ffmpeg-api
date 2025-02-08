#include "audio_mixer.hpp"
#include "utils.hpp"

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswresample/swresample.h>
}

//https://gist.github.com/royshil/fff30890c7c19a4889f0a148101c9dff
geode::Result<std::vector<float>> readAudioFile(const char *filename, int targetSampleRate, AVSampleFormat targetSampleFormat, AVCodecParameters* outCodecParams)
{
    int ret = 0;
	AVFormatContext *formatContext = nullptr;
	if (ret = avformat_open_input(&formatContext, filename, nullptr, nullptr); ret != 0)
		return geode::Err("Error opening file: " + ffmpeg::utils::getErrorString(ret));

	if (ret = avformat_find_stream_info(formatContext, nullptr); ret < 0)
        return geode::Err("Error finding stream information: " + ffmpeg::utils::getErrorString(ret));

	int audioStreamIndex = -1;
	for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
		if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			audioStreamIndex = i;
			break;
		}
	}

	if (audioStreamIndex == -1)
		return geode::Err("No audio stream found");

	AVCodecParameters *codecParams = formatContext->streams[audioStreamIndex]->codecpar;
	const AVCodec *codec = avcodec_find_decoder(codecParams->codec_id);
	if (!codec)
		return geode::Err("Decoder not found");

	AVCodecContext *codecContext = avcodec_alloc_context3(codec);
	if (!codecContext)
		return geode::Err("Failed to allocate codec context");

	if (ret = avcodec_open2(codecContext, codec, nullptr); ret < 0)
		return geode::Err("Failed to open codec: " + ffmpeg::utils::getErrorString(ret));

    if (ret = avcodec_parameters_to_context(codecContext, codecParams); ret < 0)
		return geode::Err("Failed to copy codec parameters to codec context: " + ffmpeg::utils::getErrorString(ret));

    *outCodecParams = *codecParams;

	AVFrame *frame = av_frame_alloc();
	AVPacket packet;

	AVChannelLayout ch_layout;
    av_channel_layout_from_string(&ch_layout, "2 channels");
	SwrContext *swr = nullptr;
	ret = swr_alloc_set_opts2(&swr, &ch_layout, targetSampleFormat, targetSampleRate,
				  &(codecContext->ch_layout), codecContext->sample_fmt,
				  codecContext->sample_rate, 0, nullptr);
	if (ret < 0) 
        return geode::Err("Failed to set up swr context: " + ffmpeg::utils::getErrorString(ret));
    
	ret = swr_init(swr);
	if (ret < 0)
        return geode::Err("Failed to initialize swr context: " + ffmpeg::utils::getErrorString(ret));

	std::vector<float> audioFrames;

	float *convertBuffer[2];
    convertBuffer[0] = static_cast<float *>(av_malloc(4096 * sizeof(float)));
    convertBuffer[1] = static_cast<float *>(av_malloc(4096 * sizeof(float)));

    while (av_read_frame(formatContext, &packet) >= 0) {
        if (packet.stream_index == audioStreamIndex && avcodec_send_packet(codecContext, &packet) == 0) {
                while (avcodec_receive_frame(codecContext, frame) == 0) {
                    int ret = swr_convert(swr, reinterpret_cast<uint8_t **>(convertBuffer), 4096,
                                        frame->data, frame->nb_samples);
                    if (ret < 0) 
                        return geode::Err("Failed to convert audio frame: " + ffmpeg::utils::getErrorString(ret));
                    
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

	return geode::Ok(audioFrames);
}

geode::Result<std::vector<float>> resampleAudio(const std::vector<float>& inputAudio, int inputSampleRate, int targetSampleRate) {
    SwrContext *swrCtx = nullptr;
	int ret;
    AVChannelLayout ch_layout;
    av_channel_layout_from_string(&ch_layout, "2 channels");
    
    ret = swr_alloc_set_opts2(&swrCtx, &ch_layout, AV_SAMPLE_FMT_FLT,
        targetSampleRate, &ch_layout, AV_SAMPLE_FMT_FLT,
        inputSampleRate, 0, nullptr);

    if (ret < 0) 
        return geode::Err("Failed to set up swr context: " + ffmpeg::utils::getErrorString(ret));
    
	ret = swr_init(swrCtx);
	if (ret < 0)
        return geode::Err("Failed to initialize swr context: " + ffmpeg::utils::getErrorString(ret));

    constexpr int chunkSize = 4096;
    constexpr int numChannels = 2;

    int maxOutputSamples = av_rescale_rnd(chunkSize, targetSampleRate, inputSampleRate, AV_ROUND_UP);
    std::vector<float> outputAudio;
    std::vector<float> outputChunk(maxOutputSamples * numChannels);

    const uint8_t* inData[1] = { nullptr };
    uint8_t* outData[1] = { reinterpret_cast<uint8_t*>(outputChunk.data()) };

    for (size_t i = 0; i < inputAudio.size(); i += chunkSize * numChannels) {
        size_t currentChunkSize = std::min((size_t)(chunkSize * numChannels), inputAudio.size() - i);

        inData[0] = reinterpret_cast<const uint8_t*>(&inputAudio[i]);
        int inputSamples = currentChunkSize / numChannels;

        int resampledSamples = swr_convert(swrCtx, outData, maxOutputSamples, inData, inputSamples);
        if (ret < 0)
            return geode::Err("Failed to convert audio frame: " + ffmpeg::utils::getErrorString(ret));

        outputAudio.insert(outputAudio.end(), outputChunk.begin(), outputChunk.begin() + resampledSamples * numChannels);
    }

    swr_free(&swrCtx);

    return geode::Ok(outputAudio);
}

BEGIN_FFMPEG_NAMESPACE_V
    geode::Result<> AudioMixer::mixVideoAudio(std::filesystem::path videoFile, std::filesystem::path audioFile, std::filesystem::path outputMp4File) {
        constexpr int frameSize = 1024;

        AVFormatContext* wavFormatContext = nullptr;
        int ret = 0;
        if (ret = avformat_open_input(&wavFormatContext, audioFile.string().c_str(), nullptr, nullptr); ret < 0)
            return geode::Err("Could not open file: " + utils::getErrorString(ret));

        AVCodecParameters inputAudioParams{};

        geode::Result<std::vector<float>> raw = readAudioFile(audioFile.string().c_str(), 44100, AV_SAMPLE_FMT_FLTP, &inputAudioParams);

        if(raw.isErr())
            return geode::Err(raw.unwrapErr());

        geode::Result<> res = mixVideoRaw(videoFile, raw.unwrap(), outputMp4File);

        avformat_close_input(&wavFormatContext);

        return res;
    }

    geode::Result<> AudioMixer::mixVideoRaw(const std::filesystem::path& videoFile, const std::vector<float>& raw, const std::filesystem::path &outputMp4File) {
        constexpr int frameSize = 1024;
    	constexpr uint32_t sampleRate = 44100;

        int ret = 0;

        AVFormatContext* videoFormatContext = nullptr;
        if (ret = avformat_open_input(&videoFormatContext, videoFile.string().c_str(), nullptr, nullptr); ret < 0)
            return geode::Err("Could not open MP4 file: " + utils::getErrorString(ret));

        AVFormatContext* outputFormatContext = nullptr;
        ret = avformat_alloc_output_context2(&outputFormatContext, nullptr, nullptr, outputMp4File.string().c_str());
        if (!outputFormatContext) {
            avformat_close_input(&videoFormatContext);
            return geode::Err("Could not create output context: " + utils::getErrorString(ret));
        }

        AVStream* outputVideoStream = avformat_new_stream(outputFormatContext, nullptr);
        if (!outputVideoStream)
            return geode::Err("Failed to create video stream.");

        int videoStreamIndex = -1;
        for (unsigned int i = 0; i < videoFormatContext->nb_streams; i++) {
            if (videoFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoStreamIndex = i;
                break;
            }
        }

        if(videoStreamIndex == -1)
            return geode::Err("Could not find a valid video stream.");

        AVCodecParameters* inputVideoParams = videoFormatContext->streams[videoStreamIndex]->codecpar;
        avcodec_parameters_copy(outputVideoStream->codecpar, inputVideoParams);
        outputVideoStream->codecpar->codec_tag = 0;

        AVStream* outputAudioStream = avformat_new_stream(outputFormatContext, nullptr);
        if (!outputAudioStream)
            return geode::Err("Failed to create audio stream.");

        constexpr int channels = 2;

        avformat_find_stream_info(videoFormatContext, nullptr);
        auto duration = static_cast<double>(videoFormatContext->duration) / AV_TIME_BASE;
        auto newSampleRate = raw.size() / duration / channels;

        geode::Result<std::vector<float>> resampledRes = resampleAudio(raw, newSampleRate, 44100);

        if(resampledRes.isErr())
            return geode::Err(resampledRes.unwrapErr());

        auto resampled = resampledRes.unwrap();

        outputAudioStream->codecpar->codec_tag = 0;
        outputAudioStream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
        outputAudioStream->codecpar->sample_rate = sampleRate;
        av_channel_layout_default(&outputAudioStream->codecpar->ch_layout, channels);
        outputAudioStream->codecpar->codec_id = AV_CODEC_ID_AAC;
        outputAudioStream->codecpar->bit_rate = 128000;
        outputAudioStream->codecpar->format = AVSampleFormat::AV_SAMPLE_FMT_FLTP;
        outputAudioStream->codecpar->frame_size = frameSize;

        outputFormatContext->audio_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
        outputFormatContext->audio_codec_id = AV_CODEC_ID_AAC;
        outputFormatContext->bit_rate = 128000;

        AVCodecParameters *videoCodecParams = outputAudioStream->codecpar;
	    const AVCodec *audioCodec = avcodec_find_encoder(videoCodecParams->codec_id);
        AVCodecContext *audio_codec_context_encoder = avcodec_alloc_context3(audioCodec);

        if (ret = avcodec_parameters_to_context(audio_codec_context_encoder, videoCodecParams); ret < 0)
            return geode::Err("Could not copy codec parameters to codec context: " + utils::getErrorString(ret));

        audio_codec_context_encoder->sample_rate = sampleRate;
        audio_codec_context_encoder->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
        audio_codec_context_encoder->sample_fmt = audioCodec->sample_fmts ? audioCodec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
        audio_codec_context_encoder->time_base = AVRational{1, static_cast<int>(sampleRate)};

        ret = avcodec_open2(audio_codec_context_encoder, audioCodec, nullptr);
        if (ret < 0)
            return geode::Err("Could not open encoder: " + utils::getErrorString(ret));

        if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
            if (ret = avio_open(&outputFormatContext->pb, outputMp4File.string().c_str(), AVIO_FLAG_WRITE); ret < 0) {
                avformat_free_context(outputFormatContext);
                avformat_close_input(&videoFormatContext);
                return geode::Err("Could not open output file: " + utils::getErrorString(ret));
            }
        }

        if (ret = avformat_write_header(outputFormatContext, nullptr); ret < 0) {
            avio_closep(&outputFormatContext->pb);
            avformat_free_context(outputFormatContext);
            avformat_close_input(&videoFormatContext);
            return geode::Err("Could not write header to output file: " + utils::getErrorString(ret));
        }
        
        AVPacket packet;
        while (true) {
            if (av_read_frame(videoFormatContext, &packet) >= 0) {
                av_packet_rescale_ts(&packet, videoFormatContext->streams[videoStreamIndex]->time_base, outputVideoStream->time_base);
                packet.stream_index = videoStreamIndex;
                av_interleaved_write_frame(outputFormatContext, &packet);
                av_packet_unref(&packet);
            } else {
                break;
            }
        }

        int pts = 0;

        AVFrame* audioFrame = av_frame_alloc();
        if (!audioFrame)
            return geode::Err("Could not allocate audio frame.");

        audioFrame->format = AV_SAMPLE_FMT_FLTP;
        audioFrame->ch_layout = AV_CHANNEL_LAYOUT_STEREO;

        for (size_t i = 0; i < resampled.size(); i += frameSize * channels) {
            int samplesToEncode = std::min(frameSize, static_cast<int>((resampled.size() - i) / channels));

            audioFrame->nb_samples = samplesToEncode;
            audioFrame->pts = pts;

            pts += samplesToEncode;

            if (ret = av_frame_get_buffer(audioFrame, 0); ret < 0)
                return geode::Err("Could not allocate audio buffer: " + utils::getErrorString(ret));

            for (int j = 0; j < samplesToEncode; ++j) {
                reinterpret_cast<float*>(audioFrame->data[0])[j] = resampled[i + j * channels];
                reinterpret_cast<float*>(audioFrame->data[1])[j] = resampled[i + j * channels + 1];
            }

            if (ret = avcodec_send_frame(audio_codec_context_encoder, audioFrame); ret < 0)
                return geode::Err("Could not send audio frame to encoder: " + utils::getErrorString(ret));

            AVPacket* audioPacket = av_packet_alloc();
            if (!audioPacket) return geode::Err("Failed to allocate audio packet.");

            while (true) {
                int ret = avcodec_receive_packet(audio_codec_context_encoder, audioPacket);
                if (ret == 0) {
                    av_packet_rescale_ts(audioPacket, audio_codec_context_encoder->time_base, outputAudioStream->time_base);
                    audioPacket->stream_index = outputAudioStream->index;
                    av_interleaved_write_frame(outputFormatContext, audioPacket);
                    av_packet_unref(audioPacket);
                } else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;
                else
                    return geode::Err("Could not receive audio packet: " + utils::getErrorString(ret));
            }
        }

        av_frame_free(&audioFrame);

        av_write_trailer(outputFormatContext);

        if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&outputFormatContext->pb);
        }
        avformat_free_context(outputFormatContext);
        avformat_close_input(&videoFormatContext);

        return geode::Ok();
    }
END_FFMPEG_NAMESPACE_V