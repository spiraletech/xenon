#include "xenon/media_analyzer.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#endif

namespace xenon {

const SpectrumFrame& TrackAnalysis::frameAtSeconds(double seconds) const {
    static const SpectrumFrame empty{};
    if (frames.empty() || sample_rate == 0) return empty;
    const auto index = static_cast<std::size_t>(std::max(0.0, seconds) *
        static_cast<double>(sample_rate) / static_cast<double>(kSpectrumFftSize));
    return frames[std::min(index, frames.size() - 1)];
}

TrackAnalysis MediaAnalyzer::analyzeFile(const std::filesystem::path& path) const {
#ifndef _WIN32
    (void)path;
    throw std::runtime_error("XENON MediaAnalyzer currently requires Windows Media Foundation");
#else
    IMFSourceReader* reader = nullptr;
    IMFMediaType* partial = nullptr;
    IMFMediaType* actual = nullptr;

    const auto release = [](auto*& value) {
        if (value) {
            value->Release();
            value = nullptr;
        }
    };

    HRESULT hr = MFCreateSourceReaderFromURL(path.c_str(), nullptr, &reader);
    if (FAILED(hr) || !reader) {
        throw std::runtime_error("Media Foundation could not open audio file");
    }

    reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);

    hr = MFCreateMediaType(&partial);
    if (SUCCEEDED(hr)) {
        partial->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        partial->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        hr = reader->SetCurrentMediaType(
            MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            nullptr,
            partial);
    }

    if (FAILED(hr)) {
        release(partial);
        release(reader);
        throw std::runtime_error("Media Foundation could not negotiate PCM audio");
    }

    hr = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &actual);
    UINT32 channels = 0;
    UINT32 rate = 0;
    UINT32 bits = 0;
    if (SUCCEEDED(hr) && actual) {
        actual->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
        actual->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
        actual->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits);
    }

    if (channels == 0 || rate == 0 || bits != 16) {
        release(actual);
        release(partial);
        release(reader);
        throw std::runtime_error("XENON analyzer requires decoded PCM16 audio");
    }

    TrackAnalysis result;
    result.sample_rate = rate;
    SpectrumAnalyzer analyzer;
    std::array<float, kSpectrumFftSize> window{};
    std::size_t fill = 0;
    bool done = false;

    while (!done) {
        DWORD stream = 0;
        DWORD flags = 0;
        LONGLONG timestamp = 0;
        IMFSample* sample = nullptr;
        hr = reader->ReadSample(
            MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            0,
            &stream,
            &flags,
            &timestamp,
            &sample);

        if (FAILED(hr)) {
            release(sample);
            break;
        }

        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) done = true;

        if (sample) {
            IMFMediaBuffer* buffer = nullptr;
            if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buffer)) && buffer) {
                BYTE* data = nullptr;
                DWORD max_length = 0;
                DWORD current_length = 0;
                if (SUCCEEDED(buffer->Lock(&data, &max_length, &current_length)) && data) {
                    const auto* pcm = reinterpret_cast<const std::int16_t*>(data);
                    const std::size_t pcm_frames = current_length /
                        (sizeof(std::int16_t) * channels);

                    for (std::size_t i = 0; i < pcm_frames; ++i) {
                        float mono = 0.0f;
                        for (UINT32 channel = 0; channel < channels; ++channel) {
                            mono += static_cast<float>(pcm[i * channels + channel]) / 32768.0f;
                        }
                        mono /= static_cast<float>(channels);
                        window[fill++] = mono;

                        if (fill == kSpectrumFftSize) {
                            result.frames.push_back(analyzer.analyzeWindow(window, rate));
                            fill = 0;
                            if (result.frames.size() > 30000) {
                                done = true;
                                break;
                            }
                        }
                    }
                    buffer->Unlock();
                }
                release(buffer);
            }
            release(sample);
        }
    }

    release(actual);
    release(partial);
    release(reader);

    if (result.frames.empty()) {
        throw std::runtime_error("XENON analyzer produced no spectrum frames");
    }

    analyzer.normalizeTrack(result.frames);
    return result;
#endif
}

} // namespace xenon
