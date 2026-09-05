#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace xenon {

struct Vst3ClassInfo {
    std::string name;
    std::string category;
    std::string uid;
};

struct Vst3MidiNote {
    std::int16_t pitch{60};
    float velocity{0.8f};
    std::int16_t channel{0};
    std::int32_t sample_offset{0};
    bool note_on{true};
};

class Vst3NativeHost {
public:
    Vst3NativeHost();
    ~Vst3NativeHost();
    Vst3NativeHost(const Vst3NativeHost&) = delete;
    Vst3NativeHost& operator=(const Vst3NativeHost&) = delete;
    Vst3NativeHost(Vst3NativeHost&&) noexcept;
    Vst3NativeHost& operator=(Vst3NativeHost&&) noexcept;

    [[nodiscard]] static bool sdk_enabled() noexcept;
    [[nodiscard]] static std::vector<Vst3ClassInfo> inspect(const std::filesystem::path& module_path);

    void open(const std::filesystem::path& module_path, double sample_rate, std::uint32_t max_block_frames);
    void close() noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] bool has_controller() const noexcept;
    [[nodiscard]] const std::string& plugin_name() const noexcept;

    void process(std::vector<float>& interleaved_stereo,
                 const std::vector<Vst3MidiNote>& notes = {});

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xenon
