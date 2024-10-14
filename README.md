# ffmpeg-api
A mod that lets developers easily interact with ffmpeg to record raw videos, and mix video and audio files.

## Usage

### Record videos

```cpp
#include <eclipse.ffmpeg-api/include/recorder.hpp>

void video() {
    ffmpeg::Recorder recorder;

    ffmpeg::RenderSettings settings;

    //ffmpeg-api will automatically handle conversion between the input pixel
    //format and the codec's pixel format
    settings.m_pixelFormat = PixelFormat::RGB0;
    settings.m_codecId = 27; //fetch codecs using recorder.getAvailableCodecs()
    settings.m_bitrate = 30000000;
    settings.m_width = 1920;
    settings.m_height = 1080;
    settings.m_fps = 60;
    settings.m_outputFile = "output_video.mp4";

    //insert your raw data here
    std::vector<uint8_t> frame;

    recorder.init(settings);

    for(int i = 0; i < 60; i++)
        recorder.writeFrame(frame);

    recorder.stop();
}
```

### Mix audio

```cpp
#include <eclipse.ffmpeg-api/include/audio_mixer.hpp>

void audioFile() {
    ffmpeg::AudioMixer mixer;
    mixer.mixVideoAudio("video.mp4", "audio.mp3", "output_mp3.mp4");
    mixer.mixVideoAudio("video.mp4", "audio.wav", "output_wav.mp4");
}

void audioRaw() {
    ffmpeg::AudioMixer mixer;

    //insert your raw data here
    std::vector<float> raw;
    mixer.mixVideoRaw("video.mp4", raw, "output_raw.mp4", 44100);
}
```

## Build instructions
### Windows
To get the needed libraries on Windows, you can use vcpkg:
```sh
vcpkg install ffmpeg[core,avcodec,avformat,avutil,swscale,swresample,amf,x264,x265,nvcodec]:x64-windows-static --recurse
```
the other libraries are part of the Windows SDK

## Android
To get the needed libraries on Android, you can use [this](https://github.com/EclipseMenu/ffmpeg-android-maker/) script. Read the README in the repository for further instructions.
```sh
git clone https://github.com/EclipseMenu/ffmpeg-android-maker/
cd ffmpeg-android-maker
./ffmpeg-android-maker.sh
```
You can either run it natively on Linux or using WSL on Windows.
