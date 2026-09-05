#include "xenon/vst3_native_host.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#ifdef XENON_VST3_SDK_ENABLED
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include "pluginterfaces/vst/vsttypes.h"
#endif

namespace xenon {

struct Vst3NativeHost::Impl {
    bool open{false};
    std::string name;
    double sample_rate{0.0};
    std::uint32_t max_block{0};
#ifdef XENON_VST3_SDK_ENABLED
    VST3::Hosting::Module::Ptr module;
    Steinberg::IPtr<Steinberg::Vst::PlugProvider> provider;
    Steinberg::IPtr<Steinberg::Vst::IComponent> component;
    Steinberg::IPtr<Steinberg::Vst::IAudioProcessor> processor;
#endif
};

Vst3NativeHost::Vst3NativeHost() : impl_(std::make_unique<Impl>()) {}
Vst3NativeHost::~Vst3NativeHost() { close(); }
Vst3NativeHost::Vst3NativeHost(Vst3NativeHost&&) noexcept = default;
Vst3NativeHost& Vst3NativeHost::operator=(Vst3NativeHost&&) noexcept = default;

bool Vst3NativeHost::sdk_enabled() noexcept {
#ifdef XENON_VST3_SDK_ENABLED
    return true;
#else
    return false;
#endif
}

std::vector<Vst3ClassInfo> Vst3NativeHost::inspect(const std::filesystem::path& path) {
#ifndef XENON_VST3_SDK_ENABLED
    (void)path;
    throw std::runtime_error("L41 VST3 SDK host is not enabled");
#else
    std::string error;
    auto module=VST3::Hosting::Module::create(path.string(), error);
    if (!module) throw std::runtime_error("L41 failed to load VST3 module: " + error);
    std::vector<Vst3ClassInfo> out;
    for (const auto& info : module->getFactory().classInfos()) {
        out.push_back({std::string(info.name()), std::string(info.category()), info.ID().toString()});
    }
    return out;
#endif
}

void Vst3NativeHost::open(const std::filesystem::path& path, double sample_rate, std::uint32_t max_block) {
    close();
    if (sample_rate < 8000.0 || sample_rate > 384000.0 || max_block == 0 || max_block > 65536)
        throw std::invalid_argument("L41 invalid VST3 processing configuration");
#ifndef XENON_VST3_SDK_ENABLED
    (void)path; (void)sample_rate; (void)max_block;
    throw std::runtime_error("L41 VST3 SDK host is not enabled");
#else
    std::string error;
    auto module=VST3::Hosting::Module::create(path.string(), error);
    if (!module) throw std::runtime_error("L41 failed to load VST3 module: " + error);
    const auto factory=module->getFactory();
    VST3::Hosting::ClassInfo chosen;
    bool found=false;
    for (const auto& info : factory.classInfos()) {
        if (info.category() == kVstAudioEffectClass) { chosen=info; found=true; break; }
    }
    if (!found) throw std::runtime_error("L41 VST3 module has no audio-effect class");

    auto provider=Steinberg::owned(new Steinberg::Vst::PlugProvider(factory, chosen, true));
    Steinberg::IPtr<Steinberg::Vst::IComponent> component=Steinberg::owned(provider->getComponent());
    if (!component) throw std::runtime_error("L41 VST3 component initialization failed");
    Steinberg::IPtr<Steinberg::Vst::IAudioProcessor> processor=Steinberg::U::cast<Steinberg::Vst::IAudioProcessor>(component);
    if (!processor) throw std::runtime_error("L41 VST3 component does not expose IAudioProcessor");
    if (processor->canProcessSampleSize(Steinberg::Vst::kSample32) != Steinberg::kResultTrue)
        throw std::runtime_error("L41 VST3 processor cannot process 32-bit samples");

    const Steinberg::int32 in_buses=component->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kInput);
    const Steinberg::int32 out_buses=component->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput);
    if (out_buses < 1) throw std::runtime_error("L41 VST3 processor has no audio output bus");
    Steinberg::Vst::SpeakerArrangement stereo=Steinberg::Vst::SpeakerArr::kStereo;
    if (in_buses > 0) {
        if (processor->setBusArrangements(&stereo,1,&stereo,1) != Steinberg::kResultTrue)
            throw std::runtime_error("L41 VST3 processor rejected stereo bus arrangement");
        component->activateBus(Steinberg::Vst::kAudio,Steinberg::Vst::kInput,0,true);
    } else {
        if (processor->setBusArrangements(nullptr,0,&stereo,1) != Steinberg::kResultTrue)
            throw std::runtime_error("L41 VST3 instrument rejected stereo output arrangement");
    }
    component->activateBus(Steinberg::Vst::kAudio,Steinberg::Vst::kOutput,0,true);
    const auto event_inputs=component->getBusCount(Steinberg::Vst::kEvent,Steinberg::Vst::kInput);
    if (event_inputs > 0) component->activateBus(Steinberg::Vst::kEvent,Steinberg::Vst::kInput,0,true);

    Steinberg::Vst::ProcessSetup setup{};
    setup.processMode=Steinberg::Vst::kRealtime;
    setup.symbolicSampleSize=Steinberg::Vst::kSample32;
    setup.maxSamplesPerBlock=static_cast<Steinberg::int32>(max_block);
    setup.sampleRate=sample_rate;
    if (processor->setupProcessing(setup) != Steinberg::kResultTrue)
        throw std::runtime_error("L41 VST3 setupProcessing failed");
    if (component->setActive(true) != Steinberg::kResultTrue)
        throw std::runtime_error("L41 VST3 setActive failed");
    if (processor->setProcessing(true) != Steinberg::kResultTrue) {
        component->setActive(false);
        throw std::runtime_error("L41 VST3 setProcessing failed");
    }

    impl_->module=std::move(module);
    impl_->provider=std::move(provider);
    impl_->component=std::move(component);
    impl_->processor=std::move(processor);
    impl_->name=std::string(chosen.name());
    impl_->sample_rate=sample_rate;
    impl_->max_block=max_block;
    impl_->open=true;
#endif
}

void Vst3NativeHost::close() noexcept {
    if (!impl_) return;
#ifdef XENON_VST3_SDK_ENABLED
    if (impl_->processor) impl_->processor->setProcessing(false);
    if (impl_->component) impl_->component->setActive(false);
    impl_->processor=nullptr;
    impl_->component=nullptr;
    impl_->provider=nullptr;
    impl_->module.reset();
#endif
    impl_->open=false;
    impl_->name.clear();
}

bool Vst3NativeHost::is_open() const noexcept { return impl_ && impl_->open; }
const std::string& Vst3NativeHost::plugin_name() const noexcept { return impl_->name; }

void Vst3NativeHost::process(std::vector<float>& stereo, const std::vector<Vst3MidiNote>& notes) {
#ifndef XENON_VST3_SDK_ENABLED
    (void)stereo; (void)notes;
    throw std::runtime_error("L41 VST3 SDK host is not enabled");
#else
    if (!impl_->open || !impl_->processor || !impl_->component) throw std::runtime_error("L41 VST3 processor is not open");
    if (stereo.empty() || stereo.size()%2 != 0) throw std::invalid_argument("L41 VST3 processing requires interleaved stereo samples");
    const std::size_t total_frames=stereo.size()/2;
    std::vector<float> output(stereo.size(),0.0f);
    for (std::size_t base=0;base<total_frames;base+=impl_->max_block) {
        const auto frames=static_cast<Steinberg::int32>(std::min<std::size_t>(impl_->max_block,total_frames-base));
        std::vector<float> in_l(frames),in_r(frames),out_l(frames,0.0f),out_r(frames,0.0f);
        for (Steinberg::int32 i=0;i<frames;++i) { in_l[i]=stereo[2*(base+i)]; in_r[i]=stereo[2*(base+i)+1]; }
        Steinberg::Vst::Sample32* in_channels[2]{in_l.data(),in_r.data()};
        Steinberg::Vst::Sample32* out_channels[2]{out_l.data(),out_r.data()};
        Steinberg::Vst::AudioBusBuffers input_bus{}; input_bus.numChannels=2; input_bus.channelBuffers32=in_channels;
        Steinberg::Vst::AudioBusBuffers output_bus{}; output_bus.numChannels=2; output_bus.channelBuffers32=out_channels;
        Steinberg::Vst::EventList events;
        for (const auto& n:notes) {
            if (n.sample_offset < static_cast<std::int32_t>(base) || n.sample_offset >= static_cast<std::int32_t>(base)+frames) continue;
            Steinberg::Vst::Event e{}; e.busIndex=0; e.sampleOffset=n.sample_offset-static_cast<std::int32_t>(base); e.ppqPosition=0.0; e.flags=Steinberg::Vst::Event::kIsLive;
            if (n.note_on) { e.type=Steinberg::Vst::Event::kNoteOnEvent; e.noteOn.channel=n.channel; e.noteOn.pitch=n.pitch; e.noteOn.tuning=0.f; e.noteOn.velocity=std::clamp(n.velocity,0.f,1.f); e.noteOn.length=0; e.noteOn.noteId=-1; }
            else { e.type=Steinberg::Vst::Event::kNoteOffEvent; e.noteOff.channel=n.channel; e.noteOff.pitch=n.pitch; e.noteOff.tuning=0.f; e.noteOff.velocity=std::clamp(n.velocity,0.f,1.f); e.noteOff.noteId=-1; }
            events.addEvent(e);
        }
        Steinberg::Vst::ProcessData data{}; data.processMode=Steinberg::Vst::kRealtime; data.symbolicSampleSize=Steinberg::Vst::kSample32; data.numSamples=frames;
        const auto has_audio_input=impl_->component->getBusCount(Steinberg::Vst::kAudio,Steinberg::Vst::kInput)>0;
        if (has_audio_input) { data.numInputs=1; data.inputs=&input_bus; }
        data.numOutputs=1; data.outputs=&output_bus; data.inputEvents=&events;
        if (impl_->processor->process(data) != Steinberg::kResultTrue) throw std::runtime_error("L41 VST3 process call failed");
        for (Steinberg::int32 i=0;i<frames;++i) { output[2*(base+i)]=out_l[i]; output[2*(base+i)+1]=out_r[i]; }
    }
    stereo.swap(output);
#endif
}

} // namespace xenon
