#include "xenon/player_engine.hpp"

#include <algorithm>
#include <stdexcept>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <mfapi.h>
#include <mfplay.h>
#include <propvarutil.h>
#endif

namespace xenon {

struct PlayerEngine::Impl {
#ifdef _WIN32
    IMFPMediaPlayer* player{nullptr};
    IMFPMediaItem* item{nullptr};
#endif
    TrackAnalysis analysis;
    double duration_seconds{0.0};
    bool playing{false};
};

PlayerEngine::PlayerEngine() : impl_(new Impl) {
#ifndef _WIN32
    throw std::runtime_error("XENON PlayerEngine currently requires Windows");
#endif
}

PlayerEngine::~PlayerEngine() {
#ifdef _WIN32
    if (impl_) {
        if (impl_->player) { impl_->player->Stop(); impl_->player->Shutdown(); impl_->player->Release(); }
        if (impl_->item) impl_->item->Release();
    }
#endif
    delete impl_;
}

void PlayerEngine::open(const std::filesystem::path& path) {
#ifdef _WIN32
    if (impl_->player) { impl_->player->Stop(); impl_->player->Shutdown(); impl_->player->Release(); impl_->player = nullptr; }
    if (impl_->item) { impl_->item->Release(); impl_->item = nullptr; }

    if (FAILED(MFPCreateMediaPlayer(nullptr, FALSE, 0, nullptr, nullptr, &impl_->player)) || !impl_->player)
        throw std::runtime_error("XENON could not create Media Foundation player");
    if (FAILED(impl_->player->CreateMediaItemFromURL(path.c_str(), TRUE, 0, &impl_->item)) || !impl_->item)
        throw std::runtime_error("XENON could not create media item");

    PROPVARIANT duration{};
    PropVariantInit(&duration);
    if (SUCCEEDED(impl_->item->GetDuration(MFP_POSITIONTYPE_100NS, &duration))) {
        ULONGLONG value = 0;
        if (duration.vt == VT_I8) value = static_cast<ULONGLONG>(duration.hVal.QuadPart);
        else if (duration.vt == VT_UI8) value = duration.uhVal.QuadPart;
        impl_->duration_seconds = static_cast<double>(value) / 10000000.0;
    }
    PropVariantClear(&duration);

    if (FAILED(impl_->player->SetMediaItem(impl_->item)))
        throw std::runtime_error("XENON could not attach media item");

    impl_->analysis = MediaAnalyzer{}.analyzeFile(path);
    impl_->playing = false;
#else
    (void)path;
#endif
}

void PlayerEngine::play() {
#ifdef _WIN32
    if (!impl_->player || FAILED(impl_->player->Play())) throw std::runtime_error("XENON play failed");
    impl_->playing = true;
#endif
}

void PlayerEngine::pause() {
#ifdef _WIN32
    if (impl_->player) impl_->player->Pause();
    impl_->playing = false;
#endif
}

void PlayerEngine::stop() {
#ifdef _WIN32
    if (impl_->player) impl_->player->Stop();
    impl_->playing = false;
#endif
}

void PlayerEngine::seekSeconds(double seconds) {
#ifdef _WIN32
    if (!impl_->player) return;
    PROPVARIANT value{};
    PropVariantInit(&value);
    value.vt = VT_I8;
    value.hVal.QuadPart = static_cast<LONGLONG>(std::clamp(seconds, 0.0, impl_->duration_seconds) * 10000000.0);
    impl_->player->SetPosition(MFP_POSITIONTYPE_100NS, &value);
    PropVariantClear(&value);
#else
    (void)seconds;
#endif
}

void PlayerEngine::setVolume(float volume) {
#ifdef _WIN32
    if (impl_->player) impl_->player->SetVolume(std::clamp(volume, 0.0f, 1.0f));
#else
    (void)volume;
#endif
}

double PlayerEngine::positionSeconds() const {
#ifdef _WIN32
    if (!impl_->player) return 0.0;
    PROPVARIANT value{};
    PropVariantInit(&value);
    ULONGLONG position = 0;
    if (SUCCEEDED(impl_->player->GetPosition(MFP_POSITIONTYPE_100NS, &value))) {
        if (value.vt == VT_I8) position = static_cast<ULONGLONG>(std::max<LONGLONG>(0, value.hVal.QuadPart));
        else if (value.vt == VT_UI8) position = value.uhVal.QuadPart;
    }
    PropVariantClear(&value);
    return static_cast<double>(position) / 10000000.0;
#else
    return 0.0;
#endif
}

double PlayerEngine::durationSeconds() const { return impl_->duration_seconds; }
bool PlayerEngine::isPlaying() const noexcept { return impl_->playing; }
const SpectrumFrame& PlayerEngine::currentSpectrum() const { return impl_->analysis.frameAtSeconds(positionSeconds()); }

} // namespace xenon
