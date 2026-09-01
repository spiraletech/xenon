#pragma once

#include "xenon/media_analyzer.hpp"

#include <filesystem>

namespace xenon {

class PlayerEngine {
public:
    PlayerEngine();
    ~PlayerEngine();

    PlayerEngine(const PlayerEngine&) = delete;
    PlayerEngine& operator=(const PlayerEngine&) = delete;

    void open(const std::filesystem::path& path);
    void play();
    void pause();
    void stop();
    void seekSeconds(double seconds);
    void setVolume(float volume);

    [[nodiscard]] double positionSeconds() const;
    [[nodiscard]] double durationSeconds() const;
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] const SpectrumFrame& currentSpectrum() const;

private:
    struct Impl;
    Impl* impl_{nullptr};
};

} // namespace xenon
