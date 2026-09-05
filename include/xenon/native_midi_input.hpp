#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace xenon {

struct NativeMidiMessage {
    std::uint8_t status{0};
    std::uint8_t data1{0};
    std::uint8_t data2{0};
    std::uint32_t timestamp_ms{0};
};

class NativeMidiInput {
public:
    NativeMidiInput();
    ~NativeMidiInput();
    NativeMidiInput(const NativeMidiInput&) = delete;
    NativeMidiInput& operator=(const NativeMidiInput&) = delete;
    NativeMidiInput(NativeMidiInput&&) noexcept;
    NativeMidiInput& operator=(NativeMidiInput&&) noexcept;

    void open(unsigned int input_index);
    void close() noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] std::vector<NativeMidiMessage> drain();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xenon
