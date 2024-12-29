#include "events.hpp"
#include "recorder.hpp"
#include "audio_mixer.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    using namespace ffmpeg::events::impl;

    new EventListener<EventFilter<CreateRecorderEvent>>(+[](CreateRecorderEvent* e) {
        e->setPtr(new ffmpeg::Recorder);
        return ListenerResult::Stop;
    });

    new EventListener<EventFilter<DeleteRecorderEvent>>(+[](DeleteRecorderEvent* e) {
        delete (ffmpeg::Recorder*)e->getPtr();
        return ListenerResult::Stop;
    });

    new EventListener<EventFilter<InitRecorderEvent>>(+[](InitRecorderEvent* e) {
        ffmpeg::Recorder* ptr = (ffmpeg::Recorder*)e->getPtr();
        e->setResult(ptr->init(e->getRenderSettings()));
        return ListenerResult::Stop;
    });

    new EventListener<EventFilter<StopRecorderEvent>>(+[](StopRecorderEvent* e) {
        ffmpeg::Recorder* ptr = (ffmpeg::Recorder*)e->getPtr();
        ptr->stop();
        return ListenerResult::Stop;
    });

    new EventListener<EventFilter<WriteFrameRecorderEvent>>(+[](WriteFrameRecorderEvent* e) {
        ffmpeg::Recorder* ptr = (ffmpeg::Recorder*)e->getPtr();
        e->setResult(ptr->writeFrame(e->getFrameData()));
        return ListenerResult::Stop;
    });

    new EventListener<EventFilter<CodecRecorderEvent>>(+[](CodecRecorderEvent* e) {
        e->setCodecs(ffmpeg::Recorder::getAvailableCodecs());
        return ListenerResult::Stop;
    });

    new EventListener<EventFilter<MixVideoAudioEvent>>(+[](MixVideoAudioEvent* e) {
        e->setResult(ffmpeg::AudioMixer::mixVideoAudio(e->getVideoFile(), e->getAudioFile(), e->getOutputMp4File()));
        return ListenerResult::Stop;
    });

    new EventListener<EventFilter<MixVideoRawEvent>>(+[](MixVideoRawEvent* e) {
        e->setResult(ffmpeg::AudioMixer::mixVideoRaw(e->getVideoFile(), e->getRaw(), e->getOutputMp4File()));
        return ListenerResult::Stop;
    });
}