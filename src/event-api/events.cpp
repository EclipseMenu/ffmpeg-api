#include "events.hpp"
#include "recorder.hpp"
#include "audio_mixer.hpp"

using namespace geode::prelude;

$execute {
    new EventListener<EventFilter<ffmpeg::events::CreateRecorderEvent>>(+[](ffmpeg::events::CreateRecorderEvent* e) {
        e->setPtr(new ffmpeg::Recorder);
        return ListenerResult::Stop;
    });

    new EventListener<EventFilter<ffmpeg::events::DeleteRecorderEvent>>(+[](ffmpeg::events::DeleteRecorderEvent* e) {
        delete (ffmpeg::Recorder*)e->getPtr();
        return ListenerResult::Stop;
    });

    new EventListener<EventFilter<ffmpeg::events::InitRecorderEvent>>(+[](ffmpeg::events::InitRecorderEvent* e) {
        ffmpeg::Recorder* ptr = (ffmpeg::Recorder*)e->getPtr();
        e->setResult(ptr->init(e->getRenderSettings()));
        return ListenerResult::Stop;
    });

    new EventListener<EventFilter<ffmpeg::events::StopRecorderEvent>>(+[](ffmpeg::events::StopRecorderEvent* e) {
        ffmpeg::Recorder* ptr = (ffmpeg::Recorder*)e->getPtr();
        ptr->stop();
        return ListenerResult::Stop;
    });

    new EventListener<EventFilter<ffmpeg::events::WriteFrameRecorderEvent>>(+[](ffmpeg::events::WriteFrameRecorderEvent* e) {
        ffmpeg::Recorder* ptr = (ffmpeg::Recorder*)e->getPtr();
        e->setResult(ptr->writeFrame(e->getFrameData()));
        return ListenerResult::Stop;
    });

    new EventListener<EventFilter<ffmpeg::events::CodecRecorderEvent>>(+[](ffmpeg::events::CodecRecorderEvent* e) {
        ffmpeg::Recorder* ptr = (ffmpeg::Recorder*)e->getPtr();
        e->setCodecs(ptr->getAvailableCodecs());
        return ListenerResult::Stop;
    });


    new EventListener<EventFilter<ffmpeg::events::CreateMixerEvent>>(+[](ffmpeg::events::CreateMixerEvent* e) {
        e->setPtr(new ffmpeg::AudioMixer);
        return ListenerResult::Stop;
    });

    new EventListener<EventFilter<ffmpeg::events::DeleteMixerEvent>>(+[](ffmpeg::events::DeleteMixerEvent* e) {
        delete (ffmpeg::AudioMixer*)e->getPtr();
        return ListenerResult::Stop;
    });

    new EventListener<EventFilter<ffmpeg::events::MixVideoAudioEvent>>(+[](ffmpeg::events::MixVideoAudioEvent* e) {
        ffmpeg::AudioMixer* ptr = (ffmpeg::AudioMixer*)e->getPtr();
        e->setResult(ptr->mixVideoAudio(e->getVideoFile(), e->getAudioFile(), e->getOutputMp4File()));
        return ListenerResult::Stop;
    });

    new EventListener<EventFilter<ffmpeg::events::MixVideoRawEvent>>(+[](ffmpeg::events::MixVideoRawEvent* e) {
        ffmpeg::AudioMixer* ptr = (ffmpeg::AudioMixer*)e->getPtr();
        e->setResult(ptr->mixVideoRaw(e->getVideoFile(), e->getRaw(), e->getOutputMp4File()));
        return ListenerResult::Stop;
    });
}