#include "xenon/native_midi_input.hpp"

#include <mutex>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#endif

namespace xenon {

struct NativeMidiInput::Impl {
    std::mutex mutex;
    std::vector<NativeMidiMessage> messages;
#ifdef _WIN32
    HMIDIIN handle{nullptr};
#endif
    bool open{false};
};

#ifdef _WIN32
static void CALLBACK xenon_midi_in_callback(HMIDIIN, UINT msg, DWORD_PTR instance, DWORD_PTR param1, DWORD_PTR param2) {
    if (msg != MIM_DATA || instance == 0) return;
    auto* impl = reinterpret_cast<NativeMidiInput::Impl*>(instance);
    const DWORD packed = static_cast<DWORD>(param1);
    NativeMidiMessage m;
    m.status = static_cast<std::uint8_t>(packed & 0xFFu);
    m.data1 = static_cast<std::uint8_t>((packed >> 8u) & 0x7Fu);
    m.data2 = static_cast<std::uint8_t>((packed >> 16u) & 0x7Fu);
    m.timestamp_ms = static_cast<std::uint32_t>(param2);
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (impl->messages.size() >= 4096) impl->messages.erase(impl->messages.begin(), impl->messages.begin() + 1024);
    impl->messages.push_back(m);
}
#endif

NativeMidiInput::NativeMidiInput() : impl_(std::make_unique<Impl>()) {}
NativeMidiInput::~NativeMidiInput() { close(); }
NativeMidiInput::NativeMidiInput(NativeMidiInput&& other) noexcept : impl_(std::move(other.impl_)) {
    if (!impl_) impl_ = std::make_unique<Impl>();
}
NativeMidiInput& NativeMidiInput::operator=(NativeMidiInput&& other) noexcept {
    if (this != &other) { close(); impl_ = std::move(other.impl_); if (!impl_) impl_ = std::make_unique<Impl>(); }
    return *this;
}

void NativeMidiInput::open(unsigned int index) {
    close();
#ifdef _WIN32
    if (index >= midiInGetNumDevs()) throw std::out_of_range("L41 MIDI input index out of range");
    HMIDIIN handle=nullptr;
    const auto result=midiInOpen(&handle,index,reinterpret_cast<DWORD_PTR>(&xenon_midi_in_callback),reinterpret_cast<DWORD_PTR>(impl_.get()),CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) throw std::runtime_error("L41 failed to open MIDI input device");
    impl_->handle=handle;
    impl_->open=true;
    if (midiInStart(handle) != MMSYSERR_NOERROR) { midiInClose(handle); impl_->handle=nullptr; impl_->open=false; throw std::runtime_error("L41 failed to start MIDI input device"); }
#else
    (void)index;
    throw std::runtime_error("L41 native MIDI input currently requires Windows WinMM");
#endif
}

void NativeMidiInput::close() noexcept {
    if (!impl_) return;
#ifdef _WIN32
    if (impl_->handle) { midiInStop(impl_->handle); midiInReset(impl_->handle); midiInClose(impl_->handle); impl_->handle=nullptr; }
#endif
    impl_->open=false;
}

bool NativeMidiInput::is_open() const noexcept { return impl_ && impl_->open; }
std::vector<NativeMidiMessage> NativeMidiInput::drain() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    std::vector<NativeMidiMessage> out;
    out.swap(impl_->messages);
    return out;
}

} // namespace xenon
